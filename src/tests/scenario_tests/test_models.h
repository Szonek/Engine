#pragma once
#include "scenario_harness.h"

#include <string>
#include <vector>

namespace scenario
{
// Engine tests use the generated models in assets/models/test_fixtures (see generate_test_models.py), never the game's
// models, which will be replaced.
inline constexpr const char* TEST_MODELS_DIR = "test_fixtures";

// A test model loaded as a prefab; released with this object, so a test leaves nothing behind even when an
// assertion fails. Loading can fail on purpose (the broken models), see result().
class TestModel
{
public:
    explicit TestModel(const char* file_name) : file_name_(file_name)
    {
        result_ = enginePrefabLoad(ENGINE_MODEL_SPECIFICATION_GLTF_2, file_name, TEST_MODELS_DIR, &prefab_);
    }
    TestModel(const TestModel&) = delete;
    TestModel& operator=(const TestModel&) = delete;
    ~TestModel()
    {
        if (prefab_ != ENGINE_INVALID_OBJECT_HANDLE)
        {
            enginePrefabRelease(prefab_);
        }
    }

    bool loaded() const { return result_ == ENGINE_RESULT_CODE_OK; }
    engine_result_code_t result() const { return result_; }
    engine_prefab_t prefab() const { return prefab_; }
    // the ID of an asset inside the model: "<file>#<name>"
    std::string asset(const char* name) const { return file_name_ + "#" + name; }
    engine_game_object_t instantiate() const { return enginePrefabInstantiate(prefab_); }
    engine_game_object_t instantiate_transient() const { return enginePrefabInstantiateTransient(prefab_); }

private:
    std::string file_name_;
    engine_result_code_t result_ = ENGINE_RESULT_CODE_FAIL;
    engine_prefab_t prefab_ = ENGINE_INVALID_OBJECT_HANDLE;
};

// Fixture for tests of the engine: an empty scene is active
class ModelTest : public ScenarioTest
{
protected:
    void SetUp() override { load_scene<EmptyScene>(); }

    // the children of a game object, in order
    static std::vector<engine_game_object_t> children_of(engine_game_object_t go)
    {
        std::vector<engine_game_object_t> ret;
        for (size_t i = 0; i < engineGameObjectGetChildrenCount(go); i++)
        {
            ret.push_back(engineGameObjectGetChild(go, i));
        }
        return ret;
    }

    // the child of a game object with the given name, or ENGINE_INVALID_GAME_OBJECT_ID
    static engine_game_object_t child_named(engine_game_object_t go, const std::string& name)
    {
        for (const auto child : children_of(go))
        {
            if (name == engineGameObjectGetNameComponent(child).name)
            {
                return child;
            }
        }
        return ENGINE_INVALID_GAME_OBJECT_ID;
    }
};
}  // namespace scenario
