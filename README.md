# Engine - Game Engine for ARPG game codenamed "Project C"

Welcome to Engine - game engine developed for personal learning and as a hobby project. This engine is being used to create the work-in-progress game codenamed "Project C".

## API Overview

Engine, as well as "Project C" are implemented in C++, but Engine provides a comprehensive API in C for interacting with its various systems. Key features include:

- **Application Management**: Create and manage the application lifecycle.
- **Scene Management**: Create, manage, and interact with game scenes.
- **Entity and Component Management**: Create, manipulate, and manage game entities and their components.
- **Input Handling**: Handle user input from keyboard, mouse, and touch devices.
- **Rendering and Resources**: Manage shaders, textures, and other rendering resources.
- **Physics**: Handle physics simulations, collisions, and ray casting.

### Example API Calls

Here are some example API calls to illustrate how to use the Engine's API:

```c
// Create an application
engine_application_t app;
engine_application_create_desc_t app_desc = {
    .name = "Project C",
    .asset_store_path = "./assets",
    .width = 1280,
    .height = 720,
    .fullscreen = false,
    .enable_editor = true
};
engineApplicationCreate(&app, app_desc);

// Create a scene
engine_scene_t scene;
engine_scene_create_desc_t scene_desc = {};
engineApplicationSceneCreate(app, scene_desc, &scene);

// Create a game object
engine_game_object_t game_object = engineSceneCreateGameObject(scene);

// Add a transform component to the game object
engine_tranform_component_t transform = engineSceneAddTransformComponent(scene, game_object);
transform.position[0] = 0.0f;
transform.position[1] = 0.0f;
transform.position[2] = 0.0f;
engineSceneUpdateTransformComponent(scene, game_object, &transform);

// Main loop
while (true) {
    engine_application_frame_begine_info_t frame_info = engineApplicationFrameBegin(app);
    float delta_time = frame_info.delta_time;

    // Update scene
    engineApplicationFrameSceneUpdate(app, scene, delta_time);

    // End frame
    engineApplicationFrameEnd(app);
}

// Clean up
engineApplicationSceneDestroy(app, scene);
engineApplicationDestroy(app);
```

## Installation

To get started with Engine, follow these steps:

1. **Clone the repository with submodules**:
    ```bash
    git clone --recurse-submodules https://github.com/Szonek/Engine.git
    ```
2. **Navigate to the project directory**:
    ```bash
    cd Engine
    ```
3. **Update submodules** (if necessary):
    ```bash
    git submodule update --init --recursive
    ```

## Usage

To build and run the engine:

1. **Create a build directory**:
    ```bash
    mkdir buildtree
    cd buildtree
    ```

2. **Configure the project using CMake**:
    ```bash
    cmake .. -DCMAKE_BUILD_TYPE=Release
    ```

3. **Build the project**:
    ```bash
    cmake --build . --config Release
    ```

4. **Run the "Project C" game**:
    ```bash
    ./project_c_game.exe
    ```

## Dependencies

Here is a list of the main dependencies used in Engine and their purposes:

- **RmlUi**: Used for creating and displaying user interfaces.
- **SDL**: Provides low-level access to audio, keyboard, mouse, and display functions.
- **bullet3**: A physics engine for simulating rigid body dynamics.
- **entt**: A fast and reliable entity-component system (ECS) for game development.
- **fmt**: Provides a fast and safe formatting library for C++.
- **freetype**: A font rendering library used to display text in the game.
- **glad**: A multi-language GL/GLES/EGL/GLX/WGL loader-generator.
- **glm**: A header-only C++ mathematics library for graphics software.
- **googletest**: A testing framework for writing C++ tests.
- **imgui**: A bloat-free graphical user interface library for C++.
- **imguizmo**: A small library that allow you to manipulate 4x4 float matrices.
- **rapidjson**: A fast JSON parser and generator for C++.
- **stb**: A set of single-file public domain (or MIT licensed) libraries for C/C++.
- **tinygltf**: A header-only C++ library for parsing glTF files.
- **tracy-0.10**: A real-time, nanosecond resolution, remote telemetry, hybrid frame and sampling profiler.

## Contribution

We welcome contributions from the community! To contribute to Engine:

1. Fork the repository.
2. Create a new branch with your feature or bugfix.
3. Submit a pull request with a detailed description of your changes.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for more information.