#pragma once
/*
MIT License

Copyright (c) 2021 abhilashraju

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include "GLFW/glfw3.h"
#include <functional>
#include <map>
namespace gl {
    struct GWindow
    {
        GLFWwindow* window_{ nullptr };
        GWindow(int width, int height) {
            glfwInit();
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
            window_ = glfwCreateWindow(width, height, "LearnOpenGL", NULL, NULL);
            if (window_ == NULL)
            {
                std::cout << "Failed to create GLFW window" << std::endl;
                glfwTerminate();

            }
            glfwMakeContextCurrent(window_);
            if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
            {
                std::cout << "Failed to initialize GLAD" << std::endl;
            }
            getWindowMap()[window_] = this;

            glfwSetMouseButtonCallback(window_, [](GLFWwindow* window, int button, int action , int mods) {

                auto w = getWindow(window);
                w->msContext = { button ,action,mods };
            });
        }
        ~GWindow() {
            getWindowMap()[window_] = nullptr;
            glfwDestroyWindow(window_);
        }

        GLFWwindow* window() {
            return window_;
        }
        void exec() {
            while (!glfwWindowShouldClose(window_))
            {
                // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
                // -------------------------------------------------------------------------------
                renderCallBack();
                glfwSwapBuffers(window_);
                glfwWaitEvents();
                glfwPostEmptyEvent();
            }

        }
        template<typename Callback>
        void setWindowPosCallback(Callback callback) {
            posCallBack = std::move(callback);
            glfwSetWindowPosCallback(window_, [](GLFWwindow* window, int x, int y) {
                auto w = getWindow(window);
                if (w && w->posCallBack)
                    w->posCallBack(x, y);
                });
        }
        template<typename Callback>
        void setRenderCallback(Callback callback) {
            renderCallBack = std::move(callback);
            glfwSetWindowRefreshCallback(window_, [](GLFWwindow* window) {
                auto w = getWindow(window);
                if (w && w->renderCallBack)
                    w->renderCallBack();
                });

        }
        template<typename Callback>
        void setResizeCallback(Callback callback) {
            resizeCallBack = std::move(callback);
            glfwSetWindowSizeCallback(window_, [](GLFWwindow* window, int x, int y) {
                auto w = getWindow(window);
                if (w && w->resizeCallBack)
                    w->resizeCallBack(x, y);
                });
        }
        template<typename Callback>
        void setKeyCallback(Callback callback) {
            keycallback = std::move(callback);
            glfwSetKeyCallback(window_, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
                auto w = getWindow(window);
                if (w && w->keycallback)
                    w->keycallback(key, scancode, action, mods);
                });
        }
        template<typename Callback>
        void setMouseCallback(Callback callback) {
            mousecallback = std::move(callback);
            glfwSetCursorPosCallback(window_, [](GLFWwindow* window, double x, double y) {
                auto w = getWindow(window);
                if (w && w->mousecallback)
                    w->mousecallback(w->msContext,x,y);
                });
        }
        template<typename Callback>
        void setScrollCallback(Callback callback) {
            scrollcallback = std::move(callback);
            glfwSetScrollCallback(window_, [](GLFWwindow* window, double x, double y) {
                auto w = getWindow(window);
                if (w && w->scrollcallback)
                    w->scrollcallback(x, y);
                });
        }
        struct MouseContext {
            int mousebutton{ 0 };
            int pressedstate{ GLFW_RELEASE };
            int modifiers{ 0 };
        };
        std::function<void()> posCallBack;
        std::function<void()> renderCallBack;
        std::function<void(int, int)> resizeCallBack;
        std::function<void(int key, int scancode, int action, int mods)> keycallback;
        std::function<void(const MouseContext&,double, double)> mousecallback;
        std::function<void(double, double)> scrollcallback; 
        
        MouseContext msContext;
        

        static std::map< GLFWwindow*, GWindow*>& getWindowMap() {
            static std::map< GLFWwindow*, GWindow*>  gmap;
            return gmap;
        }
        static GWindow* getWindow(GLFWwindow* win) {
            return getWindowMap()[win];
        }

    };
}

