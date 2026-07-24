# Custom Postprocess

Build exactly one custom postprocess into topst-nn-server.

Examples:

    make CUSTOM_POSTPROC=none
    make CUSTOM_POSTPROC=laneAF

Runtime behavior:

    type = obj    -> built-in detector postprocess
    type = class  -> built-in classifier postprocess
    type = custom -> selected CUSTOM_POSTPROC implementation

Add a new custom model by creating:

    custom_postproc/<name>/custom_postproc.c

The file must implement the interface declared in include/custom_postproc.h.
## laneAF

`CUSTOM_POSTPROC=laneAF` uses the model `net.so` custom postprocess entry to fill `laneaf_result_t`, then the runtime serializes it as the `lanes` JSON array.
## Rendering

Custom model overlays are selected at build time with `CUSTOM_POSTPROC=<name>`. Implement `custom_postproc_render()` in each adapter directory to draw model-specific output, such as lanes, segmentation masks, or hand keypoints.
