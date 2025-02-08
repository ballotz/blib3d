
** raster

- FILL types should not be mixed with depth fill
  to have wireframe instead of regular fill but with depth writes set independeltly, with no separate pass

- VERT RGB LIGHT works ok only for triangles
  light channels can't be coplanar in polygons with more than 3 vertices,
  means that there will be glitches during rendering when polygons are clipped
  can still use arbitrary vert rgb with triangles, or for flat shading of any polygon

** render
    
           0           3               7           10          13              17
    buf0: |xyz,xyz,xyz|xyz,xyz,xyz,xyz|xyz,xyz,xyz|xyz,xyz,xyz|xyz,xyz,xyz,xyz|xyz,xyz,xyz|
          vertex stride: 3

           0        3           7        10       13          17
    buf1: |st,st,st|st,st,st,st|st,st,st|st,st,st|st,st,st,st|st,st,st|
          vertex stride: 2
    
    face: |vertex index in buffer,number of vertices|

    meterial0
        faces: |0,3|3,4|7,3|
    meterial1
        faces: |10,3|13,4|17,3|
    