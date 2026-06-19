# topst-nn-seg

YOLOv8n segmentation demo app for AI-G/TOPST NPU runtime.

## Build

```bash
cd /home/ead/project/yolo-seg/topst-nn-seg
make
```

The app binary is:

```text
topst-nn-seg
```

## Model

The default ported model directory is:

```text
models/yolov8n_seg_quantized/
```

It contains the runtime files expected by the app:

```text
net.so
npu_cmd.bin
quantized_network.bin
```

## Run

Camera mode:

```bash
./topst-nn-seg -n models/yolov8n_seg_quantized -i camera
```

TCP RGB888 input mode:

```bash
./topst-nn-seg -n models/yolov8n_seg_quantized -i tcp -w 1280 -h 720 -j
```

Useful thresholds:

```bash
SEG_CONF_THRESH=0.25 SEG_NMS_THRESH=0.50 SEG_MASK_THRESH=0.50 ./topst-nn-seg -n models/yolov8n_seg_quantized
```

## Rebuild `net.so`

```bash
cd /home/ead/project/yolo-seg/topst-nn-seg/netso_build
make CROSS_COMPILE=aarch64-linux-gnu-
cp net.so ../models/yolov8n_seg_quantized/net.so
```

`custom_postproc/custom_postproc.c` performs YOLOv8-seg DFL decode, class sigmoid, NMS, and proto-mask composition.
