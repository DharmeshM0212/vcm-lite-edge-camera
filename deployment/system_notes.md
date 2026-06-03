# VCM-Lite Edge Camera Linux / Uno Q Deployment Notes

## Runtime split

Laptop:
- Dataset or webcam sender
- Browser dashboard client

Uno Q Linux side:
- HTTP signaling server
- WebRTC receiver
- TCP frame socket server
- C++ VCM-Lite engine
- FastAPI dashboard/control plane
- Logs and detection event snapshots

## Main runtime path

```text
Laptop dataset/webcam sender
→ HTTP signaling server on Uno Q
→ WebRTC media stream
→ Uno Q WebRTC receiver
→ local TCP JPEG frame socket
→ C++ VCM-Lite engine
→ YOLO DNN object ROI
→ VCM-lite semantic encoder
→ confidence-guided controller
→ logs / dashboard / detection event snapshots