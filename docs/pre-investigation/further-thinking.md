🧩 2. External Service = Architectural Bridge

Your “blur daemon” idea wasn’t about IPC gimmicks — it was an interface normalization layer.

What you discovered:

wlroots and smithay can both export DMA-BUFs.

Both can call Unix sockets.

Both can import blurred textures.

That’s the minimum common denominator — exactly what a shared library can exploit.

➡️ Lesson: When multiple compositor stacks can’t share code directly, the DMA-BUF + IPC abstraction becomes the lingua franca of GPU effects.
You essentially rediscovered a reusable rendering micro-service pattern for Wayland.

🧠 3. Algorithm Layer Is the True Reusable Core

From the blur-implementation-strategy.md matrix:

Hyprland → performance-tuned Kawase + vibrancy.

Wayfire → clean modular plugin system, multiple algorithms.

SceneFX → wlroots integration pattern and damage tracking.

Each solved a different layer of the problem:

Hyprland = math and shader quality

Wayfire = plugin abstraction

SceneFX = compositor interface discipline

➡️ Lesson:
The real thing worth reusing isn’t their renderers — it’s the algorithm library + parameter schema (radius, passes, tint, vibrancy, etc.).
If that layer is clean, the rest (daemon, in-client, or built-in) is just a transport choice.
It proved DMA-BUF sharing is mature enough for zero-copy use by clients.

It revealed that blur could safely happen outside the compositor without breaking security.

It clarified that the core blur library must be renderer-agnostic — because now it might run in a daemon or in a client.

| Layer                  | What You Already Have                       | What’s Next                                       |
| ---------------------- | ------------------------------------------- | ------------------------------------------------- |
| **Algorithm Core**     | extracted Kawase/Gaussian/Bokeh GLSL set    | turn into `libblurcore` (shared renderer)         |
| **IPC Protocol**       | defined `blur_ipc_request/response` structs | implement reference server + client libs          |
| **Integration Shims**  | design sketches for scroll/niri             | write minimal proof-of-concept for one compositor |
| **High-level Clients** | (optional) `libvisualeffects` idea          | treat as optional wrapper for shells              |
