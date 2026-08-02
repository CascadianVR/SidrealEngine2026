# Architecture Plan: Global Buffers + Per-Frame Instance Data

## Current State (Baseline)
- Per-mesh vertex/index buffers (`Mesh::vertexBuffer`, `Mesh::indexBuffer`)
- `PushConstants` = 144 bytes (vertexAddr + indexAddr + viewProj + model) — **exceeds 128-byte Vulkan minimum**
- `vkCmdDraw(vertexCount, 1, 0, 0)` — single draw call, single mesh, single instance
- No instancing support

## Target Architecture

```
Per-Frame Buffer (CPU-mapped, GPU-readable via device address)
┌──────────────────────┐ offset 0
│ viewProjection       │ 64 bytes
├──────────────────────┤ offset 64
│ padding              │ 192 bytes (align to 256)
├──────────────────────┤ offset 256
│ model[0] (mat4)      │ 64 bytes — instance 0 transform
│ model[1] (mat4)      │ 64 bytes — instance 1 transform
│ ...                  │
│ model[N] (mat4)      │ 64 bytes
└──────────────────────┘ size = 256 + N * 64 bytes
                         (N = MaxInstancesPerFrame, e.g. 1024)

Global Vertex Buffer  ──► [all vertices concatenated]
Global Index Buffer   ──► [all indices concatenated]
                         Mesh A: offset 0, count 1500
                         Mesh B: offset 1500, count 800
```

## Push Constants (per draw call, 40 bytes)
```
vertexBufferAddress   (8B) — global vertex buffer
indexBufferAddress    (8B) — global index buffer
vertexOffset          (4B) — index offset into global index buffer
indexCount            (4B) — how many indices to draw
instanceCount         (4B) — how many instances
firstInstance         (4B) — starting instance index in per-frame buffer
perFrameDataAddress   (8B) — address of per-frame buffer
```
**Total: 40 bytes** — comfortably under 128-byte limit.

## Shader Data Flow

```
Vertex Shader:
  1. Pull vertex from global buffer via device address:
     Vertex* vbuf = (Vertex*)gPushConstants.vertexBufferAddress;
     uint32_t* ibuf = (uint32_t*)gPushConstants.indexBufferAddress;
     Vertex v = vbuf[ ibuf[ gPushConstants.vertexOffset + vertexID ] ];

  2. Pull viewProjection from per-frame buffer:
     PerFrameBuffer* pf = (PerFrameBuffer*)gPushConstants.perFrameDataAddress;
     float4x4 vp = pf->viewProjection;

  3. Pull per-instance model from per-frame buffer:
     float4x4* models = (float4x4*)((uint64_t)pf + 256);
     float4x4 model = models[ gPushConstants.firstInstance + instanceID ];

  4. Transform: clipPos = vp * model * v.position;
```

**No descriptor sets. No bindings. All data pulled via device addresses + instance ID.**

## Per-Frame Buffer Update (each frame)

```
CPU side (VulkanCore::Render):
  1. Compute viewProjection from camera
  2. Map per-frame buffer
  3. Write viewProjection to offset 0
  4. For each draw call's instances:
     - Compute model matrix
     - Write to offset 256 + instanceIndex * 64
  5. Flush allocation
  6. Unmap
```

## Draw Call Loop

```
For each mesh in scene:
  Build PushConstants:
    vertexBufferAddress  = model.vertexBufferDeviceAddress
    indexBufferAddress   = model.indexBufferDeviceAddress
    vertexOffset         = mesh.indexOffset    (offset into global index buffer)
    indexCount           = mesh.indexCount
    instanceCount        = 1                   (or more for instancing)
    firstInstance        = currentInstanceIndex
    perFrameDataAddress  = frameResource.instanceBufferAddress

  vkCmdPushConstants(...)
  vkCmdDraw(mesh.indexCount, instanceCount, 0, firstInstance)
```

## Buffer Structure Changes

### Mesh (no GPU buffers — just metadata)
```
vertexOffset, vertexCount  (into global vertex buffer)
indexOffset, indexCount    (into global index buffer)
materialIndex
```

### Model (owns global buffers)
```
meshes[]                          (metadata only)
vertexBuffer, vertexBufferDeviceAddress   (single buffer for all meshes)
indexBuffer, indexBufferDeviceAddress
```

### FrameResource (per-frame instance data)
```
instanceBuffer, instanceBufferAddress
maxInstances, currentInstanceCount
```

## Loader Changes

1. **Phase 1**: Extract all vertex/index data from GLB into temporary `std::vector<Vertex>` / `std::vector<uint32_t>`
2. **Track offsets**: Each Mesh records `vertexOffset`, `indexOffset` into the global arrays
3. **Phase 2**: Upload one global vertex buffer and one global index buffer
4. Destroy per-mesh buffers

## Files to Modify

| File | Change |
|------|--------|
| `RendererTypes.h` | Redefine `PushConstants`, `Mesh`, `Model`, `FrameResource` |
| `Loader.h/.cpp` | Concatenate buffers, track offsets, single upload per model |
| `VulkanCore.h` | Add `MaxInstancesPerFrame`, `CreateInstanceBuffers()` |
| `VulkanCore.cpp` | Instance buffer management, per-frame upload, draw loop |
| `PipelineManager.h/.cpp` | Remove old `ShaderDataBuffer`, update push constant size |
| `shader2.slang` | Pull from per-frame buffer via device address, use `SV_InstanceID` |

## Key Constraints

- **Push constants**: Must stay ≤ 128 bytes (Vulkan minimum). Current target: 40 bytes.
- **Per-frame buffer**: Must be `HOST_ACCESS_SEQUENTIAL_WRITE` + `MAPPED` for fast CPU updates.
- **Buffer device address**: Both `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT` required on all buffers.
- **Instance buffer**: Recreated or updated every frame. Size = `256 + maxInstances * 64`.
- **Index offset**: Mesh `vertexOffset` stores offset into the **index buffer** (since indices reference vertex positions).
