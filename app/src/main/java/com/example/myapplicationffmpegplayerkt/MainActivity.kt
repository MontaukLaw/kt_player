package com.example.myapplicationffmpegplayerkt

import android.Manifest
import android.annotation.SuppressLint
import android.content.pm.PackageManager
import android.graphics.Color
import android.os.Bundle
import android.os.Environment
import android.view.SurfaceView
import android.view.WindowManager
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.example.myapplicationffmpegplayerkt.databinding.ActivityMainBinding
import java.io.File

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    private var ktplayer: KTPlayer? = null
    private var tvState: TextView? = null
    private var surfaceView: SurfaceView? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON, WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        tvState = binding.tvState
        surfaceView = binding.surfaceView
        ktplayer = KTPlayer();
        lifecycle.addObserver(ktplayer!!)

        ktplayer?.setSurfaceView(surfaceView!!)
        ktplayer?.setDataSource(File(Environment.getExternalStorageDirectory(), "demo.mp4").absolutePath)

        // 当jni有消息传递先传递给player, 然后再通过listener传给activity
        // !!断言不为空, 确保文件可以播放之后再调用cpp的播放函数
        ktplayer!!.setOnPrepareListener(object : KTPlayer.OnPreparedListener {
            @SuppressLint("SetTextI18n")
            override fun onPrepared() {
                // 如果是c++子线程调用, 则需要切换到主线程
                runOnUiThread {
                    // Toast.makeText(this@MainActivity, "准备成功，即将开始播放", Toast.LENGTH_SHORT).show();
                    tvState?.setTextColor(Color.GREEN)
                    tvState!!.text = "恭喜init成功"
                }

                ktplayer!!.start() // 调用C++ 开始播放
            }
        })

        ktplayer!!.setOnErrorListener(object : KTPlayer.OnErrorListener {
            @SuppressLint("SetTextI18n")
            override fun onError(errorCode: String?) {
                runOnUiThread {
                    tvState?.setTextColor(Color.RED)
                    tvState?.text = "初始化失败: $errorCode"
                    // Toast.makeText(this@MainActivity, "准备出错: $msg", Toast.LENGTH_SHORT).show(); }
                }
            }
        });

        checkPermission()
        // Example of a call to a native method
        // binding.sampleText.text = stringFromJNI()
    }

    // 动态申请权限
    private
    var permissions = arrayOf<String>(Manifest.permission.WRITE_EXTERNAL_STORAGE) // 如果要申请多个动态权限，此处可以写多个

    var mPermissionList: MutableList<String> = ArrayList()

    private val PERMISSION_REQUEST = 1

    // 检查权限
    private fun checkPermission() {
        mPermissionList.clear()

        // 判断哪些权限未授予
        for (permission in permissions) {
            if (ContextCompat.checkSelfPermission(this, permission) != PackageManager.PERMISSION_GRANTED) {
                mPermissionList.add(permission)
            }
        }

        // 判断是否为空
        if (mPermissionList.isEmpty()) { // 未授予的权限为空，表示都授予了
        } else {
            //请求权限方法
            val permissions = mPermissionList.toTypedArray() //将List转为数组
            ActivityCompat.requestPermissions(this, permissions, PERMISSION_REQUEST)
        }
    }

    /**
     * 响应授权
     * 这里不管用户是否拒绝，都进入首页，不再重复申请权限
     */
    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String?>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        when (requestCode) {
            PERMISSION_REQUEST -> {}
            else -> super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        }
    }

}