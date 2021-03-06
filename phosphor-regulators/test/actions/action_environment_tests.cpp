/**
 * Copyright © 2019 IBM Corporation
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
#include "action_environment.hpp"
#include "device.hpp"
#include "i2c_interface.hpp"
#include "id_map.hpp"
#include "mock_services.hpp"
#include "mocked_i2c_interface.hpp"
#include "phase_fault.hpp"
#include "rule.hpp"

#include <cstddef> // for size_t
#include <exception>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

using namespace phosphor::power::regulators;

TEST(ActionEnvironmentTests, Constructor)
{
    // Create IDMap
    IDMap idMap{};

    // Create mock services.
    MockServices services{};

    // Create Device and add to IDMap
    std::unique_ptr<i2c::I2CInterface> i2cInterface =
        i2c::create(1, 0x70, i2c::I2CInterface::InitialState::CLOSED);
    Device reg1{
        "regulator1", true,
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/reg1",
        std::move(i2cInterface)};
    idMap.addDevice(reg1);

    // Verify object state after constructor
    try
    {
        ActionEnvironment env{idMap, "regulator1", services};
        EXPECT_EQ(env.getAdditionalErrorData().size(), 0);
        EXPECT_EQ(env.getDevice().getID(), "regulator1");
        EXPECT_EQ(env.getDeviceID(), "regulator1");
        EXPECT_EQ(env.getPhaseFaults().size(), 0);
        EXPECT_EQ(env.getRuleDepth(), 0);
        EXPECT_EQ(env.getVolts().has_value(), false);
    }
    catch (const std::exception& error)
    {
        ADD_FAILURE() << "Should not have caught exception.";
    }
}

TEST(ActionEnvironmentTests, AddAdditionalErrorData)
{
    IDMap idMap{};
    MockServices services{};
    ActionEnvironment env{idMap, "", services};
    EXPECT_EQ(env.getAdditionalErrorData().size(), 0);

    env.addAdditionalErrorData("foo", "foo_value");
    env.addAdditionalErrorData("bar", "bar_value");
    EXPECT_EQ(env.getAdditionalErrorData().size(), 2);
    EXPECT_EQ(env.getAdditionalErrorData().at("foo"), "foo_value");
    EXPECT_EQ(env.getAdditionalErrorData().at("bar"), "bar_value");
}

TEST(ActionEnvironmentTests, AddPhaseFault)
{
    IDMap idMap{};
    MockServices services{};
    ActionEnvironment env{idMap, "", services};
    EXPECT_EQ(env.getPhaseFaults().size(), 0);

    // Add N phase fault
    env.addPhaseFault(PhaseFaultType::n);
    EXPECT_EQ(env.getPhaseFaults().size(), 1);
    EXPECT_EQ(env.getPhaseFaults().count(PhaseFaultType::n), 1);
    EXPECT_EQ(env.getPhaseFaults().count(PhaseFaultType::n_plus_1), 0);

    // Add N+1 phase fault
    env.addPhaseFault(PhaseFaultType::n_plus_1);
    EXPECT_EQ(env.getPhaseFaults().size(), 2);
    EXPECT_EQ(env.getPhaseFaults().count(PhaseFaultType::n), 1);
    EXPECT_EQ(env.getPhaseFaults().count(PhaseFaultType::n_plus_1), 1);

    // Add N+1 phase fault again; should be ignored since stored in a std::set
    env.addPhaseFault(PhaseFaultType::n_plus_1);
    EXPECT_EQ(env.getPhaseFaults().size(), 2);
}

TEST(ActionEnvironmentTests, DecrementRuleDepth)
{
    IDMap idMap{};
    MockServices services{};
    ActionEnvironment env{idMap, "", services};
    EXPECT_EQ(env.getRuleDepth(), 0);
    env.incrementRuleDepth("set_voltage_rule");
    env.incrementRuleDepth("set_voltage_rule");
    EXPECT_EQ(env.getRuleDepth(), 2);
    env.decrementRuleDepth();
    EXPECT_EQ(env.getRuleDepth(), 1);
    env.decrementRuleDepth();
    EXPECT_EQ(env.getRuleDepth(), 0);
    env.decrementRuleDepth();
    EXPECT_EQ(env.getRuleDepth(), 0);
}

TEST(ActionEnvironmentTests, GetAdditionalErrorData)
{
    IDMap idMap{};
    MockServices services{};
    ActionEnvironment env{idMap, "", services};
    EXPECT_EQ(env.getAdditionalErrorData().size(), 0);

    env.addAdditionalErrorData("foo", "foo_value");
    EXPECT_EQ(env.getAdditionalErrorData().size(), 1);
    EXPECT_EQ(env.getAdditionalErrorData().at("foo"), "foo_value");

    env.addAdditionalErrorData("bar", "bar_value");
    EXPECT_EQ(env.getAdditionalErrorData().size(), 2);
    EXPECT_EQ(env.getAdditionalErrorData().at("bar"), "bar_value");
}

TEST(ActionEnvironmentTests, GetDevice)
{
    // Create IDMap
    IDMap idMap{};

    // Create mock services.
    MockServices services{};

    // Create Device and add to IDMap
    std::unique_ptr<i2c::I2CInterface> i2cInterface =
        i2c::create(1, 0x70, i2c::I2CInterface::InitialState::CLOSED);
    Device reg1{
        "regulator1", true,
        "/xyz/openbmc_project/inventory/system/chassis/motherboard/reg1",
        std::move(i2cInterface)};
    idMap.addDevice(reg1);

    ActionEnvironment env{idMap, "regulator1", services};

    // Test where current device ID is in the IDMap
    try
    {
        Device& device = env.getDevice();
        EXPECT_EQ(device.getID(), "regulator1");
        EXPECT_EQ(&device, &reg1);
    }
    catch (const std::exception& error)
    {
        ADD_FAILURE() << "Should not have caught exception.";
    }

    // Test where current device ID is not in the IDMap
    env.setDeviceID("regulator2");
    try
    {
        env.getDevice();
        ADD_FAILURE() << "Should not have reached this line.";
    }
    catch (const std::invalid_argument& ia_error)
    {
        EXPECT_STREQ(ia_error.what(),
                     "Unable to find device with ID \"regulator2\"");
    }
    catch (const std::exception& error)
    {
        ADD_FAILURE() << "Should not have caught exception.";
    }
}

TEST(ActionEnvironmentTests, GetDeviceID)
{
    IDMap idMap{};
    MockServices services{};
    ActionEnvironment env{idMap, "", services};
    EXPECT_EQ(env.getDeviceID(), "");
    env.setDeviceID("regulator1");
    EXPECT_EQ(env.getDeviceID(), "regulator1");
}

TEST(ActionEnvironmentTests, GetPhaseFaults)
{
    IDMap idMap{};
    MockServices services{};
    ActionEnvironment env{idMap, "", services};
    EXPECT_EQ(env.getPhaseFaults().size(), 0);

    env.addPhaseFault(PhaseFaultType::n);
    env.addPhaseFault(PhaseFaultType::n_plus_1);
    EXPECT_EQ(env.getPhaseFaults().size(), 2);
    EXPECT_EQ(env.getPhaseFaults().count(PhaseFaultType::n), 1);
    EXPECT_EQ(env.getPhaseFaults().count(PhaseFaultType::n_plus_1), 1);
}

TEST(ActionEnvironmentTests, GetRule)
{
    // Create IDMap
    IDMap idMap{};

    // Create mock services.
    MockServices services{};

    Rule setVoltageRule{"set_voltage_rule",
                        std::vector<std::unique_ptr<Action>>{}};
    idMap.addRule(setVoltageRule);

    ActionEnvironment env{idMap, "", services};

    // Test where rule ID is in the IDMap
    try
    {
        Rule& rule = env.getRule("set_voltage_rule");
        EXPECT_EQ(rule.getID(), "set_voltage_rule");
        EXPECT_EQ(&rule, &setVoltageRule);
    }
    catch (const std::exception& error)
    {
        ADD_FAILURE() << "Should not have caught exception.";
    }

    // Test where rule ID is not in the IDMap
    try
    {
        env.getRule("set_voltage_rule2");
        ADD_FAILURE() << "Should not have reached this line.";
    }
    catch (const std::invalid_argument& ia_error)
    {
        EXPECT_STREQ(ia_error.what(),
                     "Unable to find rule with ID \"set_voltage_rule2\"");
    }
    catch (const std::exception& error)
    {
        ADD_FAILURE() << "Should not have caught exception.";
    }
}

TEST(ActionEnvironmentTests, GetRuleDepth)
{
    IDMap idMap{};
    MockServices services{};
    ActionEnvironment env{idMap, "", services};
    EXPECT_EQ(env.getRuleDepth(), 0);
    env.incrementRuleDepth("set_voltage_rule");
    EXPECT_EQ(env.getRuleDepth(), 1);
    env.incrementRuleDepth("set_voltage_rule");
    EXPECT_EQ(env.getRuleDepth(), 2);
    env.decrementRuleDepth();
    EXPECT_EQ(env.getRuleDepth(), 1);
    env.decrementRuleDepth();
    EXPECT_EQ(env.getRuleDepth(), 0);
}

TEST(ActionEnvironmentTests, GetServices)
{
    IDMap idMap{};
    MockServices services{};
    ActionEnvironment env{idMap, "", services};
    EXPECT_EQ(&(env.getServices()), &services);
}

TEST(ActionEnvironmentTests, GetVolts)
{
    IDMap idMap{};
    MockServices services{};
    ActionEnvironment env{idMap, "", services};
    EXPECT_EQ(env.getVolts().has_value(), false);
    env.setVolts(1.31);
    EXPECT_EQ(env.getVolts().has_value(), true);
    EXPECT_EQ(env.getVolts().value(), 1.31);
}

TEST(ActionEnvironmentTests, IncrementRuleDepth)
{
    IDMap idMap{};
    MockServices services{};
    ActionEnvironment env{idMap, "", services};
    EXPECT_EQ(env.getRuleDepth(), 0);

    // Test where rule depth has not exceeded maximum
    try
    {
        for (size_t i = 1; i <= env.maxRuleDepth; ++i)
        {
            env.incrementRuleDepth("set_voltage_rule");
            EXPECT_EQ(env.getRuleDepth(), i);
        }
    }
    catch (const std::exception& error)
    {
        ADD_FAILURE() << "Should not have caught exception.";
    }

    // Test where rule depth has exceeded maximum
    try
    {
        env.incrementRuleDepth("set_voltage_rule");
    }
    catch (const std::runtime_error& r_error)
    {
        EXPECT_STREQ(r_error.what(),
                     "Maximum rule depth exceeded by rule set_voltage_rule.");
    }
    catch (const std::exception& error)
    {
        ADD_FAILURE() << "Should not have caught exception.";
    }
}

TEST(ActionEnvironmentTests, SetDeviceID)
{
    IDMap idMap{};
    MockServices services{};
    ActionEnvironment env{idMap, "regulator1", services};
    EXPECT_EQ(env.getDeviceID(), "regulator1");
    env.setDeviceID("regulator2");
    EXPECT_EQ(env.getDeviceID(), "regulator2");
}

TEST(ActionEnvironmentTests, SetVolts)
{
    try
    {
        IDMap idMap{};
        MockServices services{};
        ActionEnvironment env{idMap, "", services};
        EXPECT_EQ(env.getVolts().has_value(), false);
        env.setVolts(2.35);
        EXPECT_EQ(env.getVolts().has_value(), true);
        EXPECT_EQ(env.getVolts().value(), 2.35);
    }
    catch (const std::exception& error)
    {
        ADD_FAILURE() << "Should not have caught exception.";
    }
}
