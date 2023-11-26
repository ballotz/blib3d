
** raster

- input format could be simplified assuming internal vertex stride.
  all is needed is a geometry data pointer and a pointer to an array with the number of vertices of each face

- FILL types should not be mixed with depth fill
  to have wireframe instead of regular fill but with depth writes set independeltly, with no separate pass

