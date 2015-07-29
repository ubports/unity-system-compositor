unity-system-compositor versioning scheme
-----------------------------------------

unity-system-compositor (USC) is versioned using a standard x.y.z versioning
scheme, where x is the major version, y is the minor version and z is the patch
version.

The minor version should be increased at least every time USC is changed to use
a newer mir server or mir client API/ABI. In other words, we want to guarantee
that all patch releases x.y.\* will continue to work with the same mir library
ABIs that x.y.0 was originally released against. This scheme makes it
straightforward to manage updates/fixes for released versions as patch
releases.
