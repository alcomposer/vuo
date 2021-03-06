Turns an image into a 3D object that can be added to a 3D scene. 

The 3D object is like a piece of paper that displays the image on one side.

   - `Center` — The center point of the 3D object, in Vuo coordinates.
   - `Rotation` — The rotation of the 3D object, in degrees (Euler angles). At (0,0,0), the image is facing front.
   - `Width` — The width of the 3D object, in Vuo coordinates. The height is calculated from the width to preserve the image's aspect ratio.
   - `Opacity` — The opacity of the 3D object. Ranges from 0 (fully transparent) to 1 (fully opaque). 
   - `Highlight Color` — The color of shiny (specular) reflections.
   - `Shininess` — How dull (0) or polished (1) the surface appears.

This object is lit by the lights in the scene.  (Parts of the object that face the light will be brighter than parts that face away from the light.)
