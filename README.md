# NodeMediaCore

## NodeMedia开源版RTMP客户端底层库文件  
实现了rtmp的播放和发布,各平台根据回调数据实现音视频采集和播放即可.  
原为上海依图定制的版本,也曾售予北京分享一下,现开源  
注意:非NodeMediaClient商业版core
### 依赖库
 * libsrs_rtmp
 * libfdk_aac
 * libspeex
 * libopenh264
 * libavutil
 * libyuv
 
### 特性
 * 音频解码 AAC/SPEEX/NELLYMOSER
 * 视频解码 h.264
 * 音频编码 AAC
 * 视频编码 h.264
 
