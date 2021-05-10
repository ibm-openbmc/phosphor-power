/**
 * Copyright © 2020 IBM Corporation
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
#include "action_error.hpp"
#include "compare_vpd_action.hpp"
#include "id_map.hpp"
#include "mock_services.hpp"
#include "mock_vpd.hpp"

#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::Return;
using ::testing::Throw;

using namespace phosphor::power::regulators;

TEST(CompareVPDActionTests, Constructor)
{
    CompareVPDAction action{
        "/xyz/openbmc_project/inventory/system/chassis/disk_backplane", "CCIN",
        "2D35"};
    EXPECT_EQ(action.getFRU(),
              "/xyz/openbmc_project/inventory/system/chassis/disk_backplane");
    EXPECT_EQ(action.getKeyword(), "CCIN");
    EXPECT_EQ(action.getValue(), "2D35");
}

TEST(CompareVPDActionTests, Execute)
{
    // Test where works: Actual VPD value is not an empty string
    {
        std::string fru{"/xyz/openbmc_project/inventory/system"};
        std::string keyword{"Model"};

        // Create MockServices object.  VPD service will return "ABCD" as VPD
        // value 4 times.
        MockServices services{};
        MockVPD& vpd = services.getMockVPD();
        EXPECT_CALL(vpd, getValue(fru, keyword))
            .Times(4)
            .WillRepeatedly(Return("ABCD"));

        IDMap idMap{};
        ActionEnvironment environment{idMap, "", services};

        // Test where returns true: actual value == expected value
        {
            CompareVPDAction action{fru, keyword, "ABCD"};
            EXPECT_TRUE(action.execute(environment));
        }

        // Test where returns false: actual value != expected value
        {
            CompareVPDAction action{fru, keyword, "BEEF"};
            EXPECT_FALSE(action.execute(environment));
        }

        // Test where returns false: expected value differs by case
        {
            CompareVPDAction action{fru, keyword, "abcd"};
            EXPECT_FALSE(action.execute(environment));
        }

        // Test where returns false: expected value is an empty string
        {
            CompareVPDAction action{fru, keyword, ""};
            EXPECT_FALSE(action.execute(environment));
        }
    }

    // Test where works: Actual VPD value is an empty string
    {
        std::string fru{"/xyz/openbmc_project/inventory/system"};
        std::string keyword{"Model"};

        // Create MockServices object.  VPD service will return "" as VPD value
        // 2 times.
        MockServices services{};
        MockVPD& vpd = services.getMockVPD();
        EXPECT_CALL(vpd, getValue(fru, keyword))
            .Times(2)
            .WillRepeatedly(Return(""));

        IDMap idMap{};
        ActionEnvironment environment{idMap, "", services};

        // Test where returns true: actual value == expected value
        {
            CompareVPDAction action{fru, keyword, ""};
            EXPECT_TRUE(action.execute(environment));
        }

        // Test where returns false: actual value != expected value
        {
            CompareVPDAction action{fru, keyword, "ABCD"};
            EXPECT_FALSE(action.execute(environment));
        }
    }

    // Test where fails: Exception thrown when trying to get actual VPD value
    {
        std::string fru{"/xyz/openbmc_project/inventory/system"};
        std::string keyword{"Model"};

        // Create MockServices object.  VPD service will throw an exception.
        MockServices services{};
        MockVPD& vpd = services.getMockVPD();
        EXPECT_CALL(vpd, getValue(fru, keyword))
            .Times(1)
            .WillOnce(
                Throw(std::runtime_error{"D-Bus error: Invalid object path"}));

        IDMap idMap{};
        ActionEnvironment environment{idMap, "", services};

        try
        {
            CompareVPDAction action{fru, keyword, "ABCD"};
            action.execute(environment);
            ADD_FAILURE() << "Should not have reached this line.";
        }
        catch (const ActionError& e)
        {
            EXPECT_STREQ(e.what(), "ActionError: compare_vpd: { fru: "
                                   "/xyz/openbmc_project/inventory/system, "
                                   "keyword: Model, value: ABCD }");
            try
            {
                // Re-throw inner exception
                std::rethrow_if_nested(e);
                ADD_FAILURE() << "Should not have reached this line.";
            }
            catch (const std::runtime_error& re)
            {
                EXPECT_STREQ(re.what(), "D-Bus error: Invalid object path");
            }
            catch (...)
            {
                ADD_FAILURE() << "Should not have caught exception.";
            }
        }
        catch (...)
        {
            ADD_FAILURE() << "Should not have caught exception.";
        }
    }
}

TEST(CompareVPDActionTests, GetFRU)
{
    CompareVPDAction action{
        "/xyz/openbmc_project/inventory/system/chassis/disk_backplane", "CCIN",
        "2D35"};
    EXPECT_EQ(action.getFRU(),
              "/xyz/openbmc_project/inventory/system/chassis/disk_backplane");
}

TEST(CompareVPDActionTests, GetKeyword)
{
    CompareVPDAction action{
        "/xyz/openbmc_project/inventory/system/chassis/disk_backplane", "CCIN",
        "2D35"};
    EXPECT_EQ(action.getKeyword(), "CCIN");
}

TEST(CompareVPDActionTests, GetValue)
{
    CompareVPDAction action{
        "/xyz/openbmc_project/inventory/system/chassis/disk_backplane", "CCIN",
        "2D35"};
    EXPECT_EQ(action.getValue(), "2D35");
}

TEST(CompareVPDActionTests, ToString)
{
    CompareVPDAction action{
        "/xyz/openbmc_project/inventory/system/chassis/disk_backplane", "CCIN",
        "2D35"};
    EXPECT_EQ(action.toString(), "compare_vpd: { fru: "
                                 "/xyz/openbmc_project/inventory/system/"
                                 "chassis/disk_backplane, keyword: "
                                 "CCIN, value: 2D35 }");
}
