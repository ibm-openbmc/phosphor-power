/**
 * Copyright © 2017 IBM Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <phosphor-logging/log.hpp>
#include <phosphor-logging/elog.hpp>
#include <xyz/openbmc_project/Sensor/Device/error.hpp>
#include <xyz/openbmc_project/Control/Device/error.hpp>
#include <xyz/openbmc_project/Power/Fault/error.hpp>
#include "elog-errors.hpp"
#include "names_values.hpp"
#include "power_supply.hpp"
#include "pmbus.hpp"
#include "utility.hpp"

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Control::Device::Error;
using namespace sdbusplus::xyz::openbmc_project::Sensor::Device::Error;
using namespace sdbusplus::xyz::openbmc_project::Power::Fault::Error;

namespace witherspoon
{
namespace power
{
namespace psu
{

constexpr auto INVENTORY_OBJ_PATH = "/xyz/openbmc_project/inventory";
constexpr auto INVENTORY_INTERFACE = "xyz.openbmc_project.Inventory.Item";
constexpr auto PRESENT_PROP = "Present";
constexpr auto POWER_OBJ_PATH = "/org/openbmc/control/power0";
constexpr auto POWER_INTERFACE = "org.openbmc.control.Power";

PowerSupply::PowerSupply(const std::string& name, size_t inst,
                         const std::string& objpath,
                         const std::string& invpath,
                         sdbusplus::bus::bus& bus,
                         event::Event& e,
                         std::chrono::seconds& t)
    : Device(name, inst), monitorPath(objpath), pmbusIntf(objpath),
      inventoryPath(invpath), bus(bus), event(e), powerOnInterval(t),
      powerOnTimer(e, [this]()
                   {
                       this->powerOn = true;
                   })
{
    using namespace sdbusplus::bus;
    auto present_obj_path = INVENTORY_OBJ_PATH + inventoryPath;
    presentMatch = std::make_unique<match_t>(bus,
                                             match::rules::propertiesChanged(
                                                     present_obj_path,
                                                     INVENTORY_INTERFACE),
                                             [this](auto& msg)
                                             {
                                                 this->inventoryChanged(msg);
                                             });
    // Get initial presence state.
    updatePresence();

    // Subscribe to power state changes
    powerOnMatch = std::make_unique<match_t>(bus,
                                             match::rules::propertiesChanged(
                                                     POWER_OBJ_PATH,
                                                     POWER_INTERFACE),
                                             [this](auto& msg)
                                             {
                                                 this->powerStateChanged(msg);
                                             });
    // Get initial power state.
    updatePowerState();
}

void PowerSupply::captureCmd(util::NamesValues& nv, const std::string& cmd,
                             witherspoon::pmbus::Type type)
{
    if (pmbusIntf.exists(cmd, type))
    {
        try
        {
            auto val = pmbusIntf.read(cmd, type);
            nv.add(cmd, val);
        }
        catch (std::exception& e)
        {
            log<level::INFO>("Unable to capture metadata", entry("CMD=%s",
                                                                 cmd));
        }
    }
}

void PowerSupply::analyze()
{
    using namespace witherspoon::pmbus;

    try
    {
        if (present)
        {
            std::uint16_t statusWord = 0;

            // Read the 2 byte STATUS_WORD value to check for faults.
            statusWord = pmbusIntf.read(STATUS_WORD, Type::Debug);

            //TODO: 3 consecutive reads should be performed.
            // If 3 consecutive reads are seen, log the fault.
            // Driver gives cached value, read once a second.
            // increment for fault on, decrement for fault off, to deglitch.
            // If count reaches 3, we have fault. If count reaches 0, fault is
            // cleared.

            checkInputFault(statusWord);

            if (powerOn)
            {
                checkPGOrUnitOffFault(statusWord);
                checkCurrentOutOverCurrentFault(statusWord);
                checkOutputOvervoltageFault(statusWord);
                checkFanFault(statusWord);
                checkTemperatureFault(statusWord);
            }
        }
    }
    catch (ReadFailure& e)
    {
        if (!readFailLogged)
        {
            commit<ReadFailure>();
            readFailLogged = true;
        }
    }

    return;
}

void PowerSupply::inventoryChanged(sdbusplus::message::message& msg)
{
    std::string msgSensor;
    std::map<std::string, sdbusplus::message::variant<uint32_t, bool>> msgData;
    msg.read(msgSensor, msgData);

    // Check if it was the Present property that changed.
    auto valPropMap = msgData.find(PRESENT_PROP);
    if (valPropMap != msgData.end())
    {
        present = sdbusplus::message::variant_ns::get<bool>(valPropMap->second);

        if (present)
        {
            readFailLogged = false;
            vinUVFault = false;
            inputFault = false;
            outputOCFault = false;
            outputOVFault = false;
            fanFault = false;
            temperatureFault = false;
        }
    }

    return;
}

void PowerSupply::updatePresence()
{
    // Use getProperty utility function to get presence status.
    std::string path = INVENTORY_OBJ_PATH + inventoryPath;
    std::string service = "xyz.openbmc_project.Inventory.Manager";

    try
    {
        util::getProperty(INVENTORY_INTERFACE, PRESENT_PROP, path,
                          service, bus, this->present);
    }
    catch (std::exception& e)
    {
        // If we happen to be trying to update presence just as it is being
        // updated, we may encounter a runtime_error. Just catch that for
        // now, let the inventoryChanged signal handler update presence later.
        present = false;
    }

}

void PowerSupply::powerStateChanged(sdbusplus::message::message& msg)
{
    int32_t state = 0;
    std::string msgSensor;
    std::map<std::string, sdbusplus::message::variant<int32_t, int32_t>>
            msgData;
    msg.read(msgSensor, msgData);

    // Check if it was the Present property that changed.
    auto valPropMap = msgData.find("state");
    if (valPropMap != msgData.end())
    {
        state = sdbusplus::message::variant_ns::get<int32_t>(valPropMap->second);

        // Power is on when state=1. Set the fault logged variables to false
        // and start the power on timer when the state changes to 1.
        if (state)
        {
            readFailLogged = false;
            vinUVFault = false;
            inputFault = false;
            powerOnFault = false;
            outputOCFault = false;
            outputOVFault = false;
            fanFault = false;
            temperatureFault = false;
            powerOnTimer.start(powerOnInterval, Timer::TimerType::oneshot);
        }
        else
        {
            powerOnTimer.stop();
            powerOn = false;
        }
    }

}

void PowerSupply::updatePowerState()
{
    // When state = 1, system is powered on
    int32_t state = 0;

    try
    {
        auto service = util::getService(POWER_OBJ_PATH,
                                        POWER_INTERFACE,
                                        bus);

        // Use getProperty utility function to get power state.
        util::getProperty<int32_t>(POWER_INTERFACE,
                                   "state",
                                   POWER_OBJ_PATH,
                                   service,
                                   bus,
                                   state);

        if (state)
        {
            powerOn = true;
        }
        else
        {
            powerOn = false;
        }
    }
    catch (std::exception& e)
    {
        log<level::INFO>("Failed to get power state. Assuming it is off.");
        powerOn = false;
    }

}

void PowerSupply::checkInputFault(const uint16_t statusWord)
{
    using namespace witherspoon::pmbus;

    std::uint8_t  statusInput = 0;

    if ((statusWord & status_word::VIN_UV_FAULT) && !vinUVFault)
    {
        vinUVFault = true;

        util::NamesValues nv;
        nv.add("STATUS_WORD", statusWord);

        using metadata = xyz::openbmc_project::Power::Fault::
                PowerSupplyUnderVoltageFault;

        report<PowerSupplyUnderVoltageFault>(metadata::RAW_STATUS(
                                                     nv.get().c_str()));
    }
    else
    {
        if (vinUVFault)
        {
            vinUVFault = false;
            log<level::INFO>("VIN_UV_FAULT cleared",
                             entry("POWERSUPPLY=%s",
                                     inventoryPath.c_str()));
        }
    }

    if ((statusWord & status_word::INPUT_FAULT_WARN) && !inputFault)
    {
        inputFault = true;

        util::NamesValues nv;
        nv.add("STATUS_WORD", statusWord);
        captureCmd(nv, STATUS_INPUT, Type::Debug);

        using metadata = xyz::openbmc_project::Power::Fault::
                PowerSupplyInputFault;

        report<PowerSupplyInputFault>(
                metadata::RAW_STATUS(nv.get().c_str()));
    }
    else
    {
        if ((inputFault) &&
            !(statusWord & status_word::INPUT_FAULT_WARN))
        {
            inputFault = false;
            statusInput = pmbusIntf.read(STATUS_INPUT, Type::Debug);

            log<level::INFO>("INPUT_FAULT_WARN cleared",
                             entry("POWERSUPPLY=%s", inventoryPath.c_str()),
                             entry("STATUS_WORD=0x%04X", statusWord),
                             entry("STATUS_INPUT=0x%02X", statusInput));
        }
    }
}

void PowerSupply::checkPGOrUnitOffFault(const uint16_t statusWord)
{
    using namespace witherspoon::pmbus;

    // Check PG# and UNIT_IS_OFF
    if (((statusWord & status_word::POWER_GOOD_NEGATED) ||
         (statusWord & status_word::UNIT_IS_OFF)) &&
        !powerOnFault)
    {
        util::NamesValues nv;
        nv.add("STATUS_WORD", statusWord);
        captureCmd(nv, STATUS_INPUT, Type::Debug);
        auto status0Vout = pmbusIntf.insertPageNum(STATUS_VOUT, 0);
        captureCmd(nv, status0Vout, Type::Debug);
        captureCmd(nv, STATUS_IOUT, Type::Debug);
        captureCmd(nv, STATUS_MFR, Type::Debug);

        using metadata = xyz::openbmc_project::Power::Fault::
                PowerSupplyShouldBeOn;

        // A power supply is OFF (or pgood low) but should be on.
        report<PowerSupplyShouldBeOn>(metadata::RAW_STATUS(nv.get().c_str()),
                                      metadata::CALLOUT_INVENTORY_PATH(
                                              inventoryPath.c_str()));

        powerOnFault = true;
    }

}

void PowerSupply::checkCurrentOutOverCurrentFault(const uint16_t statusWord)
{
    using namespace witherspoon::pmbus;

    // Check for an output overcurrent fault.
    if ((statusWord & status_word::IOUT_OC_FAULT) &&
        !outputOCFault)
    {
        util::NamesValues nv;
        nv.add("STATUS_WORD", statusWord);
        captureCmd(nv, STATUS_INPUT, Type::Debug);
        auto status0Vout = pmbusIntf.insertPageNum(STATUS_VOUT, 0);
        captureCmd(nv, status0Vout, Type::Debug);
        captureCmd(nv, STATUS_IOUT, Type::Debug);
        captureCmd(nv, STATUS_MFR, Type::Debug);

        using metadata = xyz::openbmc_project::Power::Fault::
                PowerSupplyOutputOvercurrent;

        report<PowerSupplyOutputOvercurrent>(metadata::RAW_STATUS(
                                                     nv.get().c_str()),
                                             metadata::CALLOUT_INVENTORY_PATH(
                                                     inventoryPath.c_str()));

        outputOCFault = true;
    }
}

void PowerSupply::checkOutputOvervoltageFault(const uint16_t statusWord)
{
    using namespace witherspoon::pmbus;

    // Check for an output overvoltage fault.
    if ((statusWord & status_word::VOUT_OV_FAULT) &&
        !outputOVFault)
    {
        util::NamesValues nv;
        nv.add("STATUS_WORD", statusWord);
        captureCmd(nv, STATUS_INPUT, Type::Debug);
        auto status0Vout = pmbusIntf.insertPageNum(STATUS_VOUT, 0);
        captureCmd(nv, status0Vout, Type::Debug);
        captureCmd(nv, STATUS_IOUT, Type::Debug);
        captureCmd(nv, STATUS_MFR, Type::Debug);

        using metadata = xyz::openbmc_project::Power::Fault::
                PowerSupplyOutputOvervoltage;

        report<PowerSupplyOutputOvervoltage>(metadata::RAW_STATUS(
                                                     nv.get().c_str()),
                                             metadata::CALLOUT_INVENTORY_PATH(
                                                     inventoryPath.c_str()));

        outputOVFault = true;
    }
}

void PowerSupply::checkFanFault(const uint16_t statusWord)
{
    using namespace witherspoon::pmbus;

    // Check for a fan fault or warning condition
    if ((statusWord & status_word::FAN_FAULT) &&
        !fanFault)
    {
        util::NamesValues nv;
        nv.add("STATUS_WORD", statusWord);
        captureCmd(nv, STATUS_MFR, Type::Debug);
        captureCmd(nv, STATUS_TEMPERATURE, Type::Debug);
        captureCmd(nv, STATUS_FANS_1_2, Type::Debug);

        using metadata = xyz::openbmc_project::Power::Fault::
                PowerSupplyFanFault;

        report<PowerSupplyFanFault>(
                metadata::RAW_STATUS(nv.get().c_str()),
                metadata::CALLOUT_INVENTORY_PATH(inventoryPath.c_str()));

        fanFault = true;
    }
}

void PowerSupply::checkTemperatureFault(const uint16_t statusWord)
{
    using namespace witherspoon::pmbus;

    // Due to how the PMBus core device driver sends a clear faults command
    // the bit in STATUS_WORD will likely be cleared when we attempt to examine
    // it for a Thermal Fault or Warning. So, check the STATUS_WORD and the
    // STATUS_TEMPERATURE bits. If either indicates a fault, proceed with
    // logging the over-temperature condition.
    std::uint8_t statusTemperature = 0;
    statusTemperature = pmbusIntf.read(STATUS_TEMPERATURE, Type::Debug);
    if (((statusWord & status_word::TEMPERATURE_FAULT_WARN) ||
         (statusTemperature & status_temperature::OT_FAULT)) &&
        !temperatureFault)
    {
        // The power supply has had an over-temperature condition.
        // This may not result in a shutdown if experienced for a short
        // duration.
        // This should not occur under normal conditions.
        // The power supply may be faulty, or the paired supply may be putting
        // out less current.
        // Capture command responses with potentially relevant information,
        // and call out the power supply reporting the condition.
        util::NamesValues nv;
        nv.add("STATUS_WORD", statusWord);
        captureCmd(nv, STATUS_MFR, Type::Debug);
        captureCmd(nv, STATUS_IOUT, Type::Debug);
        nv.add("STATUS_TEMPERATURE", statusTemperature);
        captureCmd(nv, STATUS_FANS_1_2, Type::Debug);

        using metadata = xyz::openbmc_project::Power::Fault::
                PowerSupplyTemperatureFault;

        report<PowerSupplyTemperatureFault>(
                metadata::RAW_STATUS(nv.get().c_str()),
                metadata::CALLOUT_INVENTORY_PATH(inventoryPath.c_str()));

        temperatureFault = true;
    }
}

void PowerSupply::clearFaults()
{
    //TODO - Clear faults at pre-poweron. openbmc/openbmc#1736
    return;
}

}
}
}