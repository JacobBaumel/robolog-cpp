#include <GLFW/glfw3.h>
#include <atomic>
#include <mutex>
#include <thread>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <vector>
#include <map>
#include <functional>
#include <poll.h>

int main() {
	int sock;
	sockaddr_in add;
	sock = socket(AF_INET, SOCK_STREAM, 0);
	add.sin_family = AF_INET;
	add.sin_port = htons(7503);
	inet_pton(AF_INET, "127.0.0.1", &add.sin_addr);
	if(connect(sock, (sockaddr*) &add, sizeof(add)) < 0) {
		std::cout << "failed to connect to server" << std::endl;
		return -1;
	}

	std::vector<char> buffer;
	std::mutex canAccessBuffer;
	std::atomic_bool stop{false};
	auto f = [&stop, &sock, &buffer, &canAccessBuffer]() {
		size_t length = 0;

		pollfd fd;
		memset(&fd, 0, sizeof(pollfd));
		fd.fd = sock;
		fd.events = POLLIN;

		char charBuf[256];

		while(!stop) {
			if(poll(&fd, 1, 0)) {
				int chars = read(sock, charBuf, 256);
				canAccessBuffer.lock();
				//                buffer.reserve(length + chars);                for(char c : chars) buffer.push_back(c);

				//                buffer.insert(buffer.begin() + length, charBuf, charBuf + chars);
				for(int i = 0; i < chars; i++) {
					buffer.push_back(charBuf[i]);
				}
				length += chars;
				canAccessBuffer.unlock();
			}
		}
	};

	std::thread socketThread(f);

	if(!glfwInit()) {
		std::cout << "Error initing glfw" << std::endl;
		return -1;
	}

	GLFWwindow* window = glfwCreateWindow(1920, 1080, "Robolog", nullptr, nullptr);
	if(!window) {
		std::cout << "Error creating window!" << std::endl;
		return -1;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.IniFilename = nullptr;

//	ImFont* mono = io.Fonts->AddFontFromFileTTF("./UbuntuMono/UbuntuMono-Regular.ttf", 13);
//	ImFont* bold = io.Fonts->AddFontFromFileTTF("./UbuntuMono/UbuntuMono-Bold.ttf", 13);
//	ImGui::PushFont(mono);


	std::map<char, std::function<void(std::function<void()>)>> coloringFunctions;

	coloringFunctions.emplace('E', [](std::function<void()> text) {
			ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4(1, 0, 0, 1);
			text();
			});
	coloringFunctions.emplace('N', [](std::function<void()> text) {text();});
	coloringFunctions.emplace('R', [](std::function<void()> text) {
			ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4(1, 1, 1, 1);
			text();
			});

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();

	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

		ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
		ImGuiWindowFlags f = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse;
		ImGui::Begin("Robolog", nullptr, f);

		if(ImGui::CollapsingHeader("thing1")) {
			if(!buffer.empty()) {
				canAccessBuffer.lock();
				for(size_t offset = 0; offset < buffer.size(); ) {
					size_t end = 0;
					bool newline = false;
					while(offset + end < buffer.size() && buffer[offset + end] != '\\') {
						if(buffer[offset + end] == '\n') newline = true;
						end++;
					}
					if(offset + end < buffer.size() - 1) end++;
					char color = buffer[offset] == '\\' ? buffer[offset + 1] : 'N';
					auto f = coloringFunctions[color];
					auto newliner = [newline]() {if(!newline) ImGui::SameLine();};
					if(color == 'N') f([&buffer, &newliner, offset, end]() {ImGui::TextUnformatted(&buffer.front() + offset, &buffer.front() + offset + end); newliner();});
					else f([&buffer, &newliner, offset, end]() {ImGui::TextUnformatted(&buffer.front() + offset + 2, &buffer.front() + offset + end - 1); newliner();});
					offset += end;
				}
				canAccessBuffer.unlock();
			}
		}

		ImGui::End();

	  ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	stop = true;
	socketThread.join();
	std::cout << "closing socket" << std::endl;
	close(sock);
}
