#include "scenario_harness.h"

#include <gtest/gtest.h>

namespace
{
// creating the application is slow (GL context + all prefabs), so it is done once for the whole run
class ScenarioEnvironment : public ::testing::Environment
{
public:
    void SetUp() override { scenario::create_app(); }
    void TearDown() override { scenario::destroy_app(); }
};
}  // namespace

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new ScenarioEnvironment());
    return RUN_ALL_TESTS();
}
