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
#include "device.hpp"
#include "id_map.hpp"
#include "rail.hpp"
#include "rule.hpp"

#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace phosphor::power::regulators;

TEST(IDMapTests, AddDevice)
{
    IDMap idMap{};

    // Create device
    std::string id{"vio_reg"};
    Device device{id};

    // Verify device is not initially in map
    EXPECT_THROW(idMap.getDevice(id), std::invalid_argument);

    // Add device to map
    idMap.addDevice(device);

    // Verify device is now in map
    try
    {
        Device& deviceFound = idMap.getDevice(id);
        EXPECT_EQ(deviceFound.getID(), id);
        EXPECT_EQ(&deviceFound, &device);
    }
    catch (const std::exception& error)
    {
        ADD_FAILURE() << "Should not have caught exception.";
    }

    // Verify different device is not in map
    EXPECT_THROW(idMap.getDevice("vio_reg2"), std::invalid_argument);
}

TEST(IDMapTests, AddRail)
{
    IDMap idMap{};

    // Create rail
    std::string id{"vio0"};
    Rail rail{id};

    // Verify rail is not initially in map
    EXPECT_THROW(idMap.getRail(id), std::invalid_argument);

    // Add rail to map
    idMap.addRail(rail);

    // Verify rail is now in map
    try
    {
        Rail& railFound = idMap.getRail(id);
        EXPECT_EQ(railFound.getID(), id);
        EXPECT_EQ(&railFound, &rail);
    }
    catch (const std::exception& error)
    {
        ADD_FAILURE() << "Should not have caught exception.";
    }

    // Verify different rail is not in map
    EXPECT_THROW(idMap.getRail("vcs0"), std::invalid_argument);
}

TEST(IDMapTests, AddRule)
{
    IDMap idMap{};

    // Create rule
    std::string id{"set_voltage_rule"};
    Rule rule{id, std::vector<std::unique_ptr<Action>>{}};

    // Verify rule is not initially in map
    EXPECT_THROW(idMap.getRule(id), std::invalid_argument);

    // Add rule to map
    idMap.addRule(rule);

    // Verify rule is now in map
    try
    {
        Rule& ruleFound = idMap.getRule(id);
        EXPECT_EQ(ruleFound.getID(), id);
        EXPECT_EQ(&ruleFound, &rule);
    }
    catch (const std::exception& error)
    {
        ADD_FAILURE() << "Should not have caught exception.";
    }

    // Verify different rule is not in map
    EXPECT_THROW(idMap.getRule("set_voltage_rule_page0"),
                 std::invalid_argument);
}

TEST(IDMapTests, GetDevice)
{
    IDMap idMap{};

    // Add a device to the map
    std::string id{"vio_reg"};
    Device device{id};
    idMap.addDevice(device);

    // Test where ID found in map
    try
    {
        Device& deviceFound = idMap.getDevice(id);
        EXPECT_EQ(deviceFound.getID(), id);
        EXPECT_EQ(&deviceFound, &device);
    }
    catch (const std::exception& error)
    {
        ADD_FAILURE() << "Should not have caught exception.";
    }

    // Test where ID not found in map
    try
    {
        idMap.getDevice("vcs_reg");
        ADD_FAILURE() << "Should not have reached this line.";
    }
    catch (const std::invalid_argument& ia_error)
    {
        EXPECT_STREQ(ia_error.what(),
                     "Unable to find device with ID \"vcs_reg\"");
    }
    catch (const std::exception& error)
    {
        ADD_FAILURE() << "Should not have caught exception.";
    }
}

TEST(IDMapTests, GetRail)
{
    IDMap idMap{};

    // Add a rail to the map
    std::string id{"vio0"};
    Rail rail{id};
    idMap.addRail(rail);

    // Test where ID found in map
    try
    {
        Rail& railFound = idMap.getRail(id);
        EXPECT_EQ(railFound.getID(), id);
        EXPECT_EQ(&railFound, &rail);
    }
    catch (const std::exception& error)
    {
        ADD_FAILURE() << "Should not have caught exception.";
    }

    // Test where ID not found in map
    try
    {
        idMap.getRail("vcs0");
        ADD_FAILURE() << "Should not have reached this line.";
    }
    catch (const std::invalid_argument& ia_error)
    {
        EXPECT_STREQ(ia_error.what(), "Unable to find rail with ID \"vcs0\"");
    }
    catch (const std::exception& error)
    {
        ADD_FAILURE() << "Should not have caught exception.";
    }
}

TEST(IDMapTests, GetRule)
{
    IDMap idMap{};

    // Add a rule to the map
    std::string id{"set_voltage_rule"};
    Rule rule{id, std::vector<std::unique_ptr<Action>>{}};
    idMap.addRule(rule);

    // Test where ID found in map
    try
    {
        Rule& ruleFound = idMap.getRule(id);
        EXPECT_EQ(ruleFound.getID(), id);
        EXPECT_EQ(&ruleFound, &rule);
    }
    catch (const std::exception& error)
    {
        ADD_FAILURE() << "Should not have caught exception.";
    }

    // Test where ID not found in map
    try
    {
        idMap.getRule("read_sensors_rule");
        ADD_FAILURE() << "Should not have reached this line.";
    }
    catch (const std::invalid_argument& ia_error)
    {
        EXPECT_STREQ(ia_error.what(),
                     "Unable to find rule with ID \"read_sensors_rule\"");
    }
    catch (const std::exception& error)
    {
        ADD_FAILURE() << "Should not have caught exception.";
    }
}
