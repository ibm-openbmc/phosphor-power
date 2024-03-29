#pragma once

#include "average.hpp"
#include "maximum.hpp"
#include "pmbus.hpp"
#include "record_manager.hpp"
#include "types.hpp"
#include "util.hpp"
#include "utility.hpp"

#include <gpiod.hpp>
#include <sdbusplus/bus/match.hpp>

#include <filesystem>
#include <stdexcept>

namespace phosphor::power::psu
{

#if IBM_VPD
// PMBus device driver "file name" to read for CCIN value.
constexpr auto CCIN = "ccin";
constexpr auto PART_NUMBER = "part_number";
constexpr auto FRU_NUMBER = "fru";
constexpr auto SERIAL_HEADER = "header";
constexpr auto SERIAL_NUMBER = "serial_number";
constexpr auto FW_VERSION = "fw_version";

// The D-Bus property name to update with the CCIN value.
constexpr auto MODEL_PROP = "Model";
constexpr auto PN_PROP = "PartNumber";
constexpr auto SPARE_PN_PROP = "SparePartNumber";
constexpr auto SN_PROP = "SerialNumber";
constexpr auto VERSION_PROP = "Version";

// ipzVPD Keyword sizes
static constexpr auto FL_KW_SIZE = 20;
#endif

constexpr auto LOG_LIMIT = 3;
constexpr auto DEGLITCH_LIMIT = 3;
constexpr auto PGOOD_DEGLITCH_LIMIT = 5;

/**
 * @class PowerSupply
 * Represents a PMBus power supply device.
 */
class PowerSupply
{
  public:
    PowerSupply() = delete;
    PowerSupply(const PowerSupply&) = delete;
    PowerSupply(PowerSupply&&) = delete;
    PowerSupply& operator=(const PowerSupply&) = delete;
    PowerSupply& operator=(PowerSupply&&) = delete;
    ~PowerSupply() = default;

    /**
     * @param[in] invpath - String for inventory path to use
     * @param[in] i2cbus - The bus number this power supply is on
     * @param[in] i2caddr - The 16-bit I2C address of the power supply
     * @param[in] driver - i2c driver name for power supply
     * @param[in] gpioLineName - The gpio-line-name to read for presence. See
     * https://github.com/openbmc/docs/blob/master/designs/device-tree-gpio-naming.md
     */
    PowerSupply(sdbusplus::bus::bus& bus, const std::string& invpath,
                std::uint8_t i2cbus, const std::uint16_t i2caddr,
                const std::string& driver, const std::string& gpioLineName);

    phosphor::pmbus::PMBusBase& getPMBus()
    {
        return *pmbusIntf;
    }

    GPIOInterfaceBase* getPresenceGPIO()
    {
        return presenceGPIO.get();
    }

    std::string getPresenceGPIOName() const
    {
        if (presenceGPIO != nullptr)
        {
            return presenceGPIO->getName();
        }
        else
        {
            return std::string();
        }
    }

    /**
     * Power supply specific function to analyze for faults/errors.
     *
     * Various PMBus status bits will be checked for fault conditions.
     * If a certain fault bits are on, the appropriate error will be
     * committed.
     */
    void analyze();

    /**
     * Write PMBus ON_OFF_CONFIG
     *
     * This function will be called to cause the PMBus device driver to send the
     * ON_OFF_CONFIG command. Takes one byte of data.
     *
     * @param[in] data - The ON_OFF_CONFIG data byte mask.
     */
    void onOffConfig(uint8_t data);

    /**
     * Clears all the member variables that indicate if a fault bit was seen as
     * on in the STATUS_WORD or STATUS_MFR_SPECIFIC response.
     */
    void clearFaultFlags()
    {
        inputFault = 0;
        mfrFault = 0;
        statusMFR = 0;
        vinUVFault = 0;
        cmlFault = 0;
        voutOVFault = 0;
        ioutOCFault = 0;
        voutUVFault = 0;
        fanFault = 0;
        tempFault = 0;
        pgoodFault = 0;
        psKillFault = 0;
        ps12VcsFault = 0;
        psCS12VFault = 0;
        faultLogged = false;
    }

    /**
     * @brief Function to specifically clear VIN_UV/OFF fault(s).
     *
     * The PMBus HWMON device driver has various alarm "files" to read out of
     * sysfs. Reading those files will indicate if various alarms are active or
     * not, and then specifically clear those faults that go with that alarm.
     *
     * The VIN_UV fault, indicated in STATUS_INPUT, goes with in1_lcrit_alarm.
     * When a VIN_UV fault occurs, the "Unit Off For Insufficient Input Voltage"
     * may also be active. Reading in1_lcrit_alarm should clear both fault bits,
     * resulting in the corresponding fault bits in STATUS_WORD also clearing.
     *
     * See: https://www.kernel.org/doc/html/latest/hwmon/pmbus.html
     */
    void clearVinUVFault();

    /**
     * Write PMBus CLEAR_FAULTS
     *
     * This function will be called in various situations in order to clear
     * any fault status bits that may have been set, in order to start over
     * with a clean state. Presence changes and power state changes will
     * want to clear any faults logged.
     */
    void clearFaults();

    /**
     * @brief Adds properties to the inventory.
     *
     * Reads the values from the device and writes them to the
     * associated power supply D-Bus inventory object.
     *
     * This needs to be done on startup, and each time the presence
     * state changes.
     *
     * Properties added:
     * - Serial Number
     * - Part Number
     * - CCIN (Customer Card Identification Number) - added as the Model
     * - Firmware version
     */
    void updateInventory();

    /**
     * @brief Accessor function to indicate present status
     */
    bool isPresent() const
    {
        return present;
    }

    /**
     * @brief Returns the last read value from STATUS_WORD.
     */
    uint64_t getStatusWord() const
    {
        return statusWord;
    }

    /**
     * @brief Returns the last read value from STATUS_INPUT.
     */
    uint64_t getStatusInput() const
    {
        return statusInput;
    }

    /**
     * @brief Returns the last read value from STATUS_MFR.
     */
    uint64_t getMFRFault() const
    {
        return statusMFR;
    }

    /**
     * @brief Returns the last read value from STATUS_CML.
     */
    uint64_t getStatusCML() const
    {
        return statusCML;
    }

    /**
     * @brief Returns the last read value from STATUS_VOUT.
     */
    uint64_t getStatusVout() const
    {
        return statusVout;
    }

    /**
     * @brief Returns the last value read from STATUS_IOUT.
     */
    uint64_t getStatusIout() const
    {
        return statusIout;
    }

    /**
     * @brief Returns the last value read from STATUS_FANS_1_2.
     */
    uint64_t getStatusFans12() const
    {
        return statusFans12;
    }

    /**
     * @brief Returns the last value read from STATUS_TEMPERATURE.
     */
    uint64_t getStatusTemperature() const
    {
        return statusTemperature;
    }

    /**
     * @brief Returns true if a fault was found.
     */
    bool isFaulted() const
    {
        return (hasCommFault() || (vinUVFault >= DEGLITCH_LIMIT) ||
                (inputFault >= DEGLITCH_LIMIT) ||
                (voutOVFault >= DEGLITCH_LIMIT) ||
                (ioutOCFault >= DEGLITCH_LIMIT) ||
                (voutUVFault >= DEGLITCH_LIMIT) ||
                (fanFault >= DEGLITCH_LIMIT) || (tempFault >= DEGLITCH_LIMIT) ||
                (pgoodFault >= PGOOD_DEGLITCH_LIMIT) ||
                (mfrFault >= DEGLITCH_LIMIT));
    }

    /**
     * @brief Return whether a fault has been logged for this power supply
     */
    bool isFaultLogged() const
    {
        return faultLogged;
    }

    /**
     * @brief Called when a fault for this power supply has been logged.
     */
    void setFaultLogged()
    {
        faultLogged = true;
    }

    /**
     * @brief Returns true if INPUT fault occurred.
     */
    bool hasInputFault() const
    {
        return (inputFault >= DEGLITCH_LIMIT);
    }

    /**
     * @brief Returns true if MFRSPECIFIC occurred.
     */
    bool hasMFRFault() const
    {
        return (mfrFault >= DEGLITCH_LIMIT);
    }

    /**
     * @brief Returns true if VIN_UV_FAULT occurred.
     */
    bool hasVINUVFault() const
    {
        return (vinUVFault >= DEGLITCH_LIMIT);
    }

    /**
     * @brief Returns true if VOUT_OV_FAULT occurred.
     */
    bool hasVoutOVFault() const
    {
        return (voutOVFault >= DEGLITCH_LIMIT);
    }

    /**
     * @brief Returns true if IOUT_OC fault occurred (bit 4 STATUS_BYTE).
     */
    bool hasIoutOCFault() const
    {
        return (ioutOCFault >= DEGLITCH_LIMIT);
    }

    /**
     * @brief Returns true if VOUT_UV_FAULT occurred.
     */
    bool hasVoutUVFault() const
    {
        return (voutUVFault >= DEGLITCH_LIMIT);
    }

    /**
     *@brief Returns true if fan fault occurred.
     */
    bool hasFanFault() const
    {
        return (fanFault >= DEGLITCH_LIMIT);
    }

    /**
     * @brief Returns true if TEMPERATURE fault occurred.
     */
    bool hasTempFault() const
    {
        return (tempFault >= DEGLITCH_LIMIT);
    }

    /**
     * @brief Returns true if there is a PGood fault (PGOOD# inactive, or OFF
     * bit on).
     */
    bool hasPgoodFault() const
    {
        return (pgoodFault >= PGOOD_DEGLITCH_LIMIT);
    }

    /**
     * @brief Return true if there is a PS_Kill fault.
     */
    bool hasPSKillFault() const
    {
        return (psKillFault >= DEGLITCH_LIMIT);
    }

    /**
     * @brief Returns true if there is a 12Vcs (standy power) fault.
     */
    bool hasPS12VcsFault() const
    {
        return (ps12VcsFault >= DEGLITCH_LIMIT);
    }

    /**
     * @brief Returns true if there is a 12V current-share fault.
     */
    bool hasPSCS12VFault() const
    {
        return (psCS12VFault >= DEGLITCH_LIMIT);
    }

    /**
     * @brief Returns the device path
     *
     * This can be used for error call outs.
     * Example: /sys/bus/i2c/devices/3-0068
     */
    const std::string getDevicePath() const
    {
        return pmbusIntf->path();
    }

    /**
     * @brief Returns this power supply's inventory path.
     *
     * This can be used for error call outs.
     * Example:
     * /xyz/openbmc_project/inventory/system/chassis/motherboard/powersupply1
     */
    const std::string& getInventoryPath() const
    {
        return inventoryPath;
    }

    /**
     * @brief Returns the short name (last part of inventoryPath).
     */
    const std::string& getShortName() const
    {
        return shortName;
    }

    /**
     * @brief Returns the firmware revision version read from the power supply
     */
    const std::string& getFWVersion() const
    {
        return fwVersion;
    }

    /**
     * @brief Returns the model name of the power supply
     */
    const std::string& getModelName() const
    {
        return modelName;
    }

    /** @brief Returns true if the number of failed reads exceeds limit
     * TODO: or CML bit on.
     */
    bool hasCommFault() const
    {
        return ((readFail >= LOG_LIMIT) || (cmlFault >= DEGLITCH_LIMIT));
    }

    /**
     * @brief Reads the pmbus input voltage and returns that actual voltage
     *        reading and the calculated input voltage based on thresholds.
     * @param[out] actualInputVoltage - The actual voltage reading, in Volts.
     * @param[out] inputVoltage - A rounded up/down value of the actual input
     *             voltage based on thresholds, in Volts.
     */
    void getInputVoltage(double& actualInputVoltage, int& inputVoltage) const;

    /**
     * @brief Check if the PS is considered to be available or not
     *
     * It is unavailable if any of:
     * - not present
     * - input fault active
     * - Vin UV fault active
     * - PS KILL fault active
     * - Iout OC fault active
     *
     * Other faults will, through creating error logs with callouts, already
     * be setting the Functional property to false.
     *
     * On changes, the Available property is updated in the inventory.
     */
    void checkAvailability();

    /**
     * @brief Setup for power supply input history.
     *
     * This will setup the variables and interfaces needed to get the power
     * supply input history data over to D-Bus. The only known support for this
     * at this time is the INPUT_HISTORY command implemented by the IBM Common
     * Form Factor Power Suppplies (ibm-cffps). The INPUT_HISTORY command for
     * ibm-cffps is implemented via a manufacturing specific PMBus command.
     */
    void setupInputHistory();

    /**
     * @brief Returns true if this power supply has input history (supported).
     */
    bool hasInputHistory() const
    {
        return inputHistorySupported;
    }

    /**
     * @brief Returns the number of input history records
     *
     * PowerSupply wrapper to getNumRecords() from RecordManager.
     */
    size_t getNumInputHistoryRecords() const
    {
        if (recordManager)
        {
            return recordManager->getNumRecords();
        }
        else
        {
            return 0;
        }
    }

    /**
     * @brief Returns true when INPUT_HISTORY sync is required.
     */
    bool isSyncHistoryRequired() const
    {
        return syncHistoryRequired;
    }

    /**
     * @brief Clears the indicator that sync required for INPUT_HISTORY.
     *
     * Sets variable to false to indicate that the sync is no longer required.
     * This can be used after the PSUManager has reacted to the need for the
     * INPUT_HISTORY data to be synchronized.
     */
    void clearSyncHistoryRequired()
    {
        syncHistoryRequired = false;
    }

  private:
    /** @brief systemd bus member */
    sdbusplus::bus::bus& bus;

    /** @brief Will be updated to the latest/lastvalue read from STATUS_WORD.*/
    uint64_t statusWord = 0;

    /** @brief Will be set to the last read value of STATUS_WORD. */
    uint64_t statusWordOld = 0;

    /** @brief Will be updated to the latest/lastvalue read from STATUS_INPUT.*/
    uint64_t statusInput = 0;

    /** @brief Will be updated to the latest/lastvalue read from STATUS_MFR.*/
    uint64_t statusMFR = 0;

    /** @brief Will be updated to the latest/last value read from STATUS_CML.*/
    uint64_t statusCML = 0;

    /** @brief Will be updated to the latest/last value read from STATUS_VOUT.*/
    uint64_t statusVout = 0;

    /** @brief Will be updated to the latest/last value read from STATUS_IOUT.*/
    uint64_t statusIout = 0;

    /** @brief Will be updated to the latest/last value read from
     * STATUS_FANS_1_2. */
    uint64_t statusFans12 = 0;

    /** @brief Will be updated to the latest/last value read from
     * STATUS_TEMPERATURE.*/
    uint64_t statusTemperature = 0;

    /** @brief Will be updated with latest converted value read from READ_VIN */
    int inputVoltage = phosphor::pmbus::in_input::VIN_VOLTAGE_0;

    /** @brief Will be updated with the actual voltage last read from READ_VIN
     */
    double actualInputVoltage = 0;

    /** @brief True if an error for a fault has already been logged. */
    bool faultLogged = false;

    /** @brief Incremented if bit 1 of STATUS_WORD low byte is on.
     *
     * Considered faulted if reaches DEGLITCH_LIMIT.
     */
    size_t cmlFault = 0;

    /** @brief Incremented if bit 5 of STATUS_WORD high byte is on.
     *
     * Considered faulted if reaches DEGLITCH_LIMIT.
     */
    size_t inputFault = 0;

    /** @brief Incremented if bit 4 of STATUS_WORD high byte is on.
     *
     * Considered faulted if reaches DEGLITCH_LIMIT.
     */
    size_t mfrFault = 0;

    /** @brief Incremented if bit 3 of STATUS_WORD low byte is on.
     *
     * Considered faulted if reaches DEGLITCH_LIMIT.
     */
    size_t vinUVFault = 0;

    /** @brief Incremented if bit 5 of STATUS_WORD low byte is on.
     *
     * Considered faulted if reaches DEGLITCH_LIMIT.
     */
    size_t voutOVFault = 0;

    /** @brief Incremented if bit 4 of STATUS_WORD low byte is on.
     *
     * Considered faulted if reaches DEGLITCH_LIMIT.
     */
    size_t ioutOCFault = 0;

    /** @brief Incremented if bit 7 of STATUS_WORD high byte is on and bit 5
     * (VOUT_OV) of low byte is off.
     *
     * Considered faulted if reaches DEGLITCH_LIMIT.
     */
    size_t voutUVFault = 0;

    /** @brief Incremented if FANS fault/warn bit on in STATUS_WORD.
     *
     * Considered faulted if reaches DEGLITCH_LIMIT.
     */
    size_t fanFault = 0;

    /** @brief Incremented if bit 2 of STATUS_WORD low byte is on.
     *
     * Considered faulted if reaches DEGLITCH_LIMIT. */
    size_t tempFault = 0;

    /**
     * @brief Incremented if bit 11 or 6 of STATUS_WORD is on. PGOOD# is
     * inactive, or the unit is off.
     *
     * Considered faulted if reaches DEGLITCH_LIMIT.
     */
    size_t pgoodFault = 0;

    /**
     * @brief Power Supply Kill fault.
     *
     * Incremented based on bits in STATUS_MFR_SPECIFIC. IBM power supplies use
     * bit 4 to indicate this fault. Considered faulted if it reaches
     * DEGLITCH_LIMIT.
     */
    size_t psKillFault = 0;

    /**
     * @brief Power Supply 12Vcs fault (standby power).
     *
     * Incremented based on bits in STATUS_MFR_SPECIFIC. IBM power supplies use
     * bit 6 to indicate this fault. Considered faulted if it reaches
     * DEGLITCH_LIMIT.
     */
    size_t ps12VcsFault = 0;

    /**
     * @brief Power Supply Current-Share fault in 12V domain.
     *
     * Incremented based on bits in STATUS_MFR_SPECIFIC. IBM power supplies use
     * bit 7 to indicate this fault. Considered faulted if it reaches
     * DEGLITCH_LIMIT.
     */
    size_t psCS12VFault = 0;

    /** @brief Count of the number of read failures. */
    size_t readFail = 0;

    /**
     * @brief Examine STATUS_WORD for CML (communication, memory, logic fault).
     */
    void analyzeCMLFault();

    /**
     * @brief Examine STATUS_WORD for INPUT bit on.
     *
     * "An input voltage, input current, or input power fault or warning has
     * occurred."
     */
    void analyzeInputFault();

    /**
     * @brief Examine STATUS_WORD for VOUT being set.
     *
     * If VOUT is on, "An output voltage fault or warning has occurred.", and
     * VOUT_OV_FAULT is on, there is an output over-voltage fault.
     */
    void analyzeVoutOVFault();

    /*
     * @brief Examine STATUS_WORD value read for IOUT_OC_FAULT.
     *
     * "An output overcurrent fault has occurred." If it is on, and fault not
     * set, trace STATUS_WORD, STATUS_MFR_SPECIFIC, and STATUS_IOUT values.
     */
    void analyzeIoutOCFault();

    /**
     * @brief Examines STATUS_WORD value read to see if there is a UV fault.
     *
     * Checks if the VOUT bit is on, indicating "An output voltage fault or
     * warning has occurred", if it is on, but VOUT_OV_FAULT is off, it is
     * determined to be an indication of an output under-voltage fault.
     */
    void analyzeVoutUVFault();

    /**
     * @brief Examine STATUS_WORD for the fan fault/warning bit.
     *
     * If fanFault is not on, trace that the bit now came on, include
     * STATUS_WORD, STATUS_MFR_SPECIFIC, and STATUS_FANS_1_2 values as well, to
     * help with understanding what may have caused it to be set.
     */
    void analyzeFanFault();

    /**
     * @brief Examine STATUS_WORD for temperature fault.
     */
    void analyzeTemperatureFault();

    /**
     * @brief Examine STATUS_WORD for pgood or unit off faults.
     */
    void analyzePgoodFault();

    /**
     * @brief Determine possible manufacturer-specific faults from bits in
     * STATUS_MFR.
     *
     * The bits in the STATUS_MFR_SPECIFIC command response have "Manufacturer
     * Defined" meanings. Determine which faults, if any, are present based on
     * the power supply (device driver) type.
     */
    void determineMFRFault();

    /**
     * @brief Examine STATUS_WORD value read for MFRSPECIFIC bit on.
     *
     * "A manufacturer specific fault or warning has occurred."
     *
     * If it is on, call the determineMFRFault() helper function to examine the
     * value read from STATUS_MFR_SPECIFIC.
     */
    void analyzeMFRFault();

    /**
     * @brief Analyzes the STATUS_WORD for a VIN_UV_FAULT indicator.
     */
    void analyzeVinUVFault();

    /**
     * @brief D-Bus path to use for this power supply's inventory status.
     **/
    std::string inventoryPath;

    /**
     * @brief Store the short name to avoid string processing.
     *
     * The short name will be something like powersupply1, the last part of the
     * inventoryPath.
     */
    std::string shortName;

    /**
     * @brief Given a full inventory path, returns the last node of the path as
     * the "short name"
     */
    std::string findShortName(const std::string& invPath)
    {
        auto const lastSlashPos = invPath.find_last_of('/');

        if ((lastSlashPos == std::string::npos) ||
            ((lastSlashPos + 1) == invPath.size()))
        {
            return invPath;
        }
        else
        {
            return invPath.substr(lastSlashPos + 1);
        }
    }

    /**
     * @brief The libgpiod object for monitoring PSU presence
     */
    std::unique_ptr<GPIOInterfaceBase> presenceGPIO = nullptr;

    /** @brief True if the power supply is present. */
    bool present = false;

    /** @brief Power supply model name. */
    std::string modelName;

    /** @brief D-Bus match variable used to subscribe to Present property
     * changes.
     **/
    std::unique_ptr<sdbusplus::bus::match_t> presentMatch;

    /** @brief D-Bus match variable used to subscribe for Present property
     * interface added.
     */
    std::unique_ptr<sdbusplus::bus::match_t> presentAddedMatch;

    /**
     * @brief Pointer to the PMBus interface
     *
     * Used to read or write to/from PMBus power supply devices.
     */
    std::unique_ptr<phosphor::pmbus::PMBusBase> pmbusIntf = nullptr;

    /** @brief Stored copy of the firmware version/revision string */
    std::string fwVersion;

    /**
     * @brief The file system path used for binding the device driver.
     */
    const std::filesystem::path bindPath;

    /* @brief The string to pass in for binding the device driver. */
    std::string bindDevice;

    /**
     * @brief The result of the most recent availability check
     *
     * Saved on the object so changes can be detected.
     */
    bool available = false;

    /**
     * @brief Binds or unbinds the power supply device driver
     *
     * Called when a presence change is detected to either bind the device
     * driver for the power supply when it is installed, or unbind the device
     * driver when the power supply is removed.
     *
     * Writes <device> to <path>/bind (or unbind)
     *
     * @param present - when true, will bind the device driver
     *                  when false, will unbind the device driver
     */
    void bindOrUnbindDriver(bool present);

    /**
     *  @brief Updates the presence status by querying D-Bus
     *
     * The D-Bus inventory properties for this power supply will be read to
     * determine if the power supply is present or not and update this
     * object's present member variable to reflect current status.
     **/
    void updatePresence();

    /**
     * @brief Updates the power supply presence by reading the GPIO line.
     */
    void updatePresenceGPIO();

    /**
     * @brief Callback for inventory property changes
     *
     * Process change of Present property for power supply.
     *
     * This is used if we are watching the D-Bus properties instead of reading
     * the GPIO presence line ourselves.
     *
     * @param[in]  msg - Data associated with Present change signal
     **/
    void inventoryChanged(sdbusplus::message::message& msg);

    /**
     * @brief Callback for inventory property added.
     *
     * Process add of the interface with the Present property for power supply.
     *
     * This is used if we are watching the D-Bus properties instead of reading
     * the GPIO presence line ourselves.
     *
     * @param[in]  msg - Data associated with Present add signal
     **/
    void inventoryAdded(sdbusplus::message::message& msg);

    /**
     * @brief Reads the pmbus MFR_POUT_MAX value.
     *
     * "The MFR_POUT_MAX command sets or retrieves the maximum rated output
     * power, in watts, that the unit is rated to supply."
     *
     * @return max_power_out value converted from string.
     */
    auto getMaxPowerOut() const;

    /**
     * @brief Reads the most recent input history record from the power supply
     * and updates the average and maximum properties in D-Bus if there is a new
     * reading available.
     *
     * This will still run every time analyze() is called so code can post new
     * data as soon as possible and the timestamp will more accurately reflect
     * the correct time.
     *
     * D-Bus is only updated if there is a change and the oldest record will be
     * pruned if the property already contains the max number of records.
     */
    void updateHistory();

    /**
     * @brief Set to true if INPUT_HISTORY command supported.
     *
     * Not all power supplies will support the INPUT_HISTORY command. The IBM
     * Common Form Factor power supplies do support this command.
     */
    bool inputHistorySupported{false};

    /**
     * @brief Set to true when INPUT_HISTORY sync is required.
     *
     * A power supply will need to synchronize its INPUT_HISTORY data with the
     * other power supplies installed in the system when it goes from missing to
     * present.
     */
    bool syncHistoryRequired{false};

    /**
     * @brief Class that manages the input power history records.
     **/
    std::unique_ptr<history::RecordManager> recordManager;

    /**
     * @brief The D-Bus object for the average input power history
     **/
    std::unique_ptr<history::Average> average;

    /**
     * @brief The D-Bus object for the maximum input power history
     **/
    std::unique_ptr<history::Maximum> maximum;

    /**
     * @brief The base D-Bus object path to use for the average and maximum
     * objects.
     **/
    std::string historyObjectPath;
};

} // namespace phosphor::power::psu
