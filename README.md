# CS430Project3
In this project you will color objects based on the shading model
we discussed in class.

Your program should be resistant to errors and should not segfault or produce undefined
behavior. If an error occurs, it should print a message to stderr with “Error:” prefixed to a
descriptive error message before returning a non-zero error code. I have a test suite designed to
test the robustness of your program.

Your program (raycast) should have this usage pattern:

raycast width height input.json output.ppm

The JSON data file should support all the primitives from project 2 and should implement a new
light primitive.
