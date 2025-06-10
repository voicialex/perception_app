// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

/**
 * @file test_nosignal_optimization.cpp
 * @brief 无信号画面管理优化测试程序
 * 
 * 此程序用于测试CVWindow类中新增的无信号画面管理功能，
 * 验证职责分离优化后的显示功能是否正常工作。
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include "utils/CVWindow.hpp"

std::atomic<bool> g_shouldExit{false};
std::atomic<int> g_counter{0};

// 在单独线程中计数和打印状态信息
void statusThread(const ob_smpl::CVWindow& window) {
    while (!g_shouldExit) {
        g_counter++;
        
        // 每5秒输出一次状态信息
        if (g_counter % 50 == 0) {
            std::cout << "程序运行中... 是否显示无信号画面: " 
                      << (window.isShowingNoSignalFrame() ? "是" : "否") 
                      << " (已运行 " << g_counter/10 << " 秒)" << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    std::cout << "=== 无信号画面管理优化测试 ===" << std::endl;
    std::cout << "正在测试CVWindow类的无信号画面管理功能..." << std::endl;
    
    try {
        // 创建CVWindow实例
        ob_smpl::CVWindow window("无信号画面测试窗口", 640, 480, ob_smpl::ARRANGE_SINGLE);
        
        std::cout << "\n1. 初始状态测试" << std::endl;
        std::cout << "   CVWindow应该在构造时自动显示无信号画面" << std::endl;
        
        // 显示初始窗口内容
        window.updateWindow();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        std::cout << "\n2. 测试showNoSignalFrame()方法" << std::endl;
        std::cout << "   手动调用显示无信号画面" << std::endl;
        window.showNoSignalFrame();
        window.updateWindow();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        std::cout << "\n3. 测试isShowingNoSignalFrame()方法" << std::endl;
        bool showing = window.isShowingNoSignalFrame();
        std::cout << "   当前是否显示无信号画面: " << (showing ? "是" : "否") << std::endl;
        
        std::cout << "\n4. 测试hideNoSignalFrame()方法" << std::endl;
        std::cout << "   隐藏无信号画面" << std::endl;
        window.hideNoSignalFrame();
        window.updateWindow();
        bool showingAfterHide = window.isShowingNoSignalFrame();
        std::cout << "   隐藏后是否还在显示: " << (showingAfterHide ? "是" : "否") << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        std::cout << "\n5. 测试updateNoSignalFrame()方法" << std::endl;
        std::cout << "   重新显示并更新时间戳" << std::endl;
        window.showNoSignalFrame();
        window.updateWindow();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        window.updateNoSignalFrame();
        window.updateWindow();
        std::cout << "   时间戳已更新" << std::endl;
        
        std::cout << "\n=== 交互测试阶段 ===" << std::endl;
        std::cout << "窗口现在显示无信号画面，您可以观察以下功能：" << std::endl;
        std::cout << "- 时间戳每秒自动更新" << std::endl;
        std::cout << "- 窗口正常显示'Waiting for signal...'文字" << std::endl;
        std::cout << "- 按ESC键退出测试" << std::endl;
        std::cout << "- 按1-5键测试不同的显示模式" << std::endl;
        std::cout << "\n重要提示：请观察窗口右下角的时间戳是否每秒自动更新！" << std::endl;
        
        // 启动状态监控线程
        std::thread monitor(statusThread, std::ref(window));
        
        // 在主线程中处理窗口事件和显示
        bool running = true;
        while(running) {
            // 处理窗口事件
            running = window.processEvents();
            
            // 显示窗口（必须在主线程中执行）
            window.updateWindow();
            
            // 短暂延迟，减少CPU占用
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // 停止状态监控线程
        g_shouldExit = true;
        if(monitor.joinable()) {
            monitor.join();
        }
        
        std::cout << "\n=== 测试完成 ===" << std::endl;
        std::cout << "无信号画面管理优化测试已成功完成！" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "测试过程中发生错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 