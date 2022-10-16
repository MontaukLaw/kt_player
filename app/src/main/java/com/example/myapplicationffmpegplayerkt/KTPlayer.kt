package com.example.myapplicationffmpegplayerkt

import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.LifecycleOwner

class KTPlayer : SurfaceHolder.Callback, LifecycleEventObserver {
    companion object {
        init {
            System.loadLibrary("native-lib")
        }
    }

    private var onPreparedListener: OnPreparedListener? = null  // C++层准备情况的接口
    private var onErrorListener: OnErrorListener? = null  // C++层错误情况的接口

    private var surfaceHolder: SurfaceHolder? = null

    // player cpp的对象指针
    private var nativePlayerObj: Long? = null

    fun setSurfaceView(surfaceView: SurfaceView) {
        if(surfaceHolder != null) {
            surfaceHolder?.removeCallback(this)
        }
        this.surfaceHolder = surfaceView.holder
        this.surfaceHolder?.addCallback(this)
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        setSurfaceNative(nativePlayerObj!!, holder.surface)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
    }
    // 构造
    // fun KTPlayer() {}

    private var dataSource: String = ""

    fun setDataSource(path: String) {
        this.dataSource = path
    }

    fun prepare() {
        // 调用jni的prepare方法
        prepareNative(dataSource)
    }

    fun stop() {
        stopNative(nativePlayerObj!!)
    }

    fun start() {
        // 调用jni的start方法
        startNative(nativePlayerObj!!)
    }

    fun release() {
        // 调用jni的release方法
        releaseNative(nativePlayerObj!!)
    }

    fun setOnPrepareListener(onPrepareListener: OnPreparedListener) {
        this.onPreparedListener = onPrepareListener
    }

    // 将准备成功的回调接口暴露给Activity, 方便让用户得知准备成功的消息
    interface OnPreparedListener {
        fun onPrepared()
    }

    interface OnErrorListener {
        fun onError(errorCode: String?)
    }

    fun setOnErrorListener(onErrorListener: OnErrorListener) {
        this.onErrorListener = onErrorListener
    }

    // 下面都是native函数
    private external fun prepareNative(dataSource: String): Long
    private external fun startNative(nativeObj: Long)
    private external fun stopNative(nativeObj: Long)
    private external fun releaseNative(nativeObj: Long)
    private external fun setSurfaceNative(nativeObj: Long, surface: Surface)

    // 由ActivityThread Handler统一处理
    override fun onStateChanged(source: LifecycleOwner, event: Lifecycle.Event) {
        when (event) {
            Lifecycle.Event.ON_RESUME -> {
                // 这里相当于安卓主线程来调用jni的准备函数, 千万不可以阻塞
                prepare()
            }
            Lifecycle.Event.ON_PAUSE -> {
                stop()
            }
            Lifecycle.Event.ON_DESTROY -> {
                release()
            }
            else -> {
            }
        }
    }

    // 下面两个函数是供C++层调用的
    ////////////////////////////////////////////////////////////////////////////////////////////
    // 给jni调用, 准备错误的函数
    fun onError(errorCode: Int, ffmpegError: String) {
        val title = "\nFFMPEG错误如下\n";
        if (null != onErrorListener) {
            var msg: String? = null
            when (errorCode) {
                FFMPEG.FFMPEG_CAN_NOT_OPEN_URL -> msg = "无法打开媒体文件$title$ffmpegError"
                FFMPEG.FFMPEG_CAN_NOT_FIND_STREAMS -> msg = "无法找到媒体流信息$title$ffmpegError"
                FFMPEG.FFMPEG_FIND_DECODER_FAIL -> msg = "无法找到解码器$title$ffmpegError"
                FFMPEG.FFMPEG_ALLOC_CODEC_CONTEXT_FAIL -> msg = "无法分配解码上下文$title$ffmpegError"
                FFMPEG.FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL -> msg = "根据流信息配置解码器上下文失败$title$ffmpegError"
                FFMPEG.FFMPEG_OPEN_DECODER_FAIL -> msg = "打开解码器失败$title$ffmpegError"
                FFMPEG.FFMPEG_NO_MEDIA -> msg = "没有音视频$title$ffmpegError"
            }
            onErrorListener!!.onError(msg)
        }
    }

    // 给jni反射用, jni调用这个接口, 触发后, 传到Activity
    fun onPrepared() {
        if (onPreparedListener != null) {
            onPreparedListener!!.onPrepared()
        }
    }

}