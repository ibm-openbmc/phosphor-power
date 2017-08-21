#pragma once

#include <experimental/filesystem>
#include <string>
#include <vector>

namespace witherspoon
{
namespace pmbus
{

namespace fs = std::experimental::filesystem;

/**
 * If the access should be done in the base
 * device directory, the hwmon directory, the
 * pmbus debug directory, or the device debug
 * directory.
 */
enum class Type
{
    Base,
    Hwmon,
    Debug,
    DeviceDebug
};

/**
 * @class PMBus
 *
 * This class is an interface to communicating with PMBus devices
 * by reading and writing sysfs files.
 *
 * Based on the Type parameter, the accesses can either be done
 * in the base device directory (the one passed into the constructor),
 * or in the hwmon directory for the device.
 */
class PMBus
{
    public:

        PMBus() = delete;
        ~PMBus() = default;
        PMBus(const PMBus&) = default;
        PMBus& operator=(const PMBus&) = default;
        PMBus(PMBus&&) = default;
        PMBus& operator=(PMBus&&) = default;

        /**
         * Constructor
         *
         * @param[in] path - path to the sysfs directory
         */
        PMBus(const std::string& path) :
            basePath(path)
        {
            findHwmonDir();
        }

        /**
         * Constructor
         *
         * This version is required when DeviceDebug
         * access will be used.
         *
         * @param[in] path - path to the sysfs directory
         * @param[in] driverName - the device driver name
         * @param[in] instance - chip instance number
         */
        PMBus(const std::string& path,
              const std::string& driverName,
              size_t instance) :
            basePath(path),
            driverName(driverName),
            instance(instance)
        {
            findHwmonDir();
        }

        /**
         * Reads a file in sysfs that represents a single bit,
         * therefore doing a PMBus read.
         *
         * @param[in] name - path concatenated to
         *                   basePath to read
         * @param[in] type - Path type
         *
         * @return bool - false if result was 0, else true
         */
        bool readBit(const std::string& name, Type type);

        /**
         * Reads a file in sysfs that represents a single bit,
         * where the page number passed in is substituted
         * into the name in place of the 'P' character in it.
         *
         * @param[in] name - path concatenated to
         *                   basePath to read
         * @param[in] page - page number
         * @param[in] type - Path type
         *
         * @return bool - false if result was 0, else true
         */
        bool readBitInPage(const std::string& name,
                           size_t page,
                           Type type);
        /**
         * Read byte(s) from file in sysfs.
         *
         * @param[in] name   - path concatenated to basePath to read
         * @param[in] type   - Path type
         *
         * @return uint64_t - Up to 8 bytes of data read from file.
         */
        uint64_t read(const std::string& name, Type type);

        /**
         * Writes an integer value to the file, therefore doing
         * a PMBus write.
         *
         * @param[in] name - path concatenated to
         *                   basePath to write
         * @param[in] value - the value to write
         * @param[in] type - Path type
         */
        void write(const std::string& name, int value, Type type);

        /**
         * Returns the sysfs base path of this device
         */
        inline const auto& path() const
        {
            return basePath;
        }

        /**
         * Replaces the 'P' in the string passed in with
         * the page number passed in.
         *
         * For example:
         *   insertPageNum("inP_enable", 42)
         *   returns "in42_enable"
         *
         * @param[in] templateName - the name string, with a 'P' in it
         * @param[in] page - the page number to insert where the P was
         *
         * @return string - the new string with the page number in it
         */
        static std::string insertPageNum(const std::string& templateName,
                                         size_t page);

        /**
         * Finds the path relative to basePath to the hwmon directory
         * for the device and stores it in hwmonRelPath.
         */
         void findHwmonDir();

        /**
         * Returns the path to use for the passed in type.
         *
         * @param[in] type - Path type
         *
         * @return fs::path - the full path
         */
         fs::path getPath(Type type);

    private:

        /**
         * The sysfs device path
         */
        fs::path basePath;

        /**
         * The directory name under the basePath hwmon directory
         */
        fs::path hwmonDir;

        /**
         * The device driver name.  Used for finding the device
         * debug directory.  Not required if that directory
         * isn't used.
         */
        std::string driverName;

        /**
         * The device instance number.
         *
         * Used in conjuction with the driver name for finding
         * the debug directory.  Not required if that directory
         * isn't used.
         */
        size_t instance = 0;

        /**
         * The pmbus debug path with status files
         */
        const fs::path debugPath = "/sys/kernel/debug/";

};

}
}