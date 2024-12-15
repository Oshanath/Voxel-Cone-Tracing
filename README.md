# Improving Voxel Cone Tracing

- This program is created to demonstrate the novel approach to voxelization, improving on the works of Cyril Crassin on his paper "Interactive Indirect Illumination Using Voxel Cone Tracing".

- Find the short paper written on the findings of this research project and the explanation of the algorithm and the methodology [here](https://drive.google.com/file/d/1CKZfmaXiU7Hj4zT_atpUx4u_88u7LkUX/view?usp=sharing).

- Find the demo video [here](https://youtu.be/pnq1g1u1nTk)

- To run the program, recursively clone the repository and build and run using CMake.

- To load models, download and extract [this](https://drive.google.com/file/d/12P3SfXWZ04OeYo2ctPHXmsAOS7CF14az/view?usp=sharing) folder to the `bin` directory. The models are now in `bin/models`. Then change the source code in the function `VCTRenderer::load_objects()` in the file `VCTRenderer.cpp`.

## Features
All the following features can be turned on and off using the ImGUI interface.

1. Visualizing the Voxelization result
2. Resolution of the Voxel Grid
3. Type of voxelization. Either 'Geometry' or 'Compute' corresponding to Geometry shader voxelization, which is the existing method, and Compute Shader Voxelization, the novel method.
4. The large triangle threshold. This value dictates how many voxels are processed in each of the two passes of the compute shader voxelization. All triangles that have more voxels than this threshold will be processed in the second pass.
5. Ambient Occlusion
6. Ambient Occlusion visualization
7. Occlusion decay factor - How much does the occlusion decay as the sampling point is further from the starting point?
8. Surface offset. The starting point of cones are offset from the surface to avoid collisions between the cone and the cone's starting point on the surface.
9. Cone cutoff.
