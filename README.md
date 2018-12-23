# Mesh Viewer

Platform: Windows

IDE: Visual Studio 2015

Third-party libraries: Glut, Glui

Compile method: Just choose Build to compile this project file.

The Mesh Viewer is an easy to use lightweight application for displaying three dimensional models (triangle meshes). It uses OpenGL to render the models. The files are given in Hugues Hoppe’s M file format. Triangle meshes can be displayed solid, as a wire frame (all lines or just front lines), or as point clouds. The surface normals of the triangles can be displayed optionally. Loaded models can be rotated, translated and scaled (all done with the mouse). The model is lighted by one or multiple light sources. Users can also change the type of the projection (orthogonal projection and perspective projection).

## M-File Parser

Write the code to load and explain the data. The M files are a fairly simple format that can describe all sorts of shapes, and a subset of their functionality is handled. Below is the example of an M file.

### it’s a cap model

```````
Vertex 1 -0.00418561 0.93191 4.22368
Vertex 2 -0.492633 1.82421 3.84173
……
Vertex 186 4.43596 -3.0351 0.881848
Face 1 1 7 2
Face 2 2 8 3
……
Face 324 166 37 66
Face 325 166 66 186
```````

I/O is implemented using C++ standard library functions.

The half-edge data structure is implemented to store the triangle meshes.

The ground, coordinate axes and bounding box are drew.

The model is rendered with lighting and color.

The user is allowed to choose either orthogonal projection or perspective projection.

The model can be rendered in different modes, such as:
* Points
* Wireframe rendering
* Flat shading
* Smooth shading

A mouse-based camera control is implemented to allow the user to rotate, translate and zoom in on the object:
* To rotate the object, the user clicks the left button, and moves the mouse.
* To translate the object, the user clicks the middle button, and moves the mouse.
* To zoom in/out, the user clicks the right button and moves the mouse.
