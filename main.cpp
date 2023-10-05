// having issue where deleted sections cant be used again
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

struct section {
	std::string name;
	std::vector<char> buffer;
	mutable bool mouse;
};


std::map<char, std::function<void()>> coloringFunctions;
std::vector<std::string> toDelete;

void drawSection(const section& sect);

int main() {
	int sock;
	sockaddr_in add;
	sock = socket(AF_INET, SOCK_STREAM, 0);
	add.sin_family = AF_INET;
	add.sin_port = htons(7502);
	inet_pton(AF_INET, "127.0.0.1", &add.sin_addr);
	if(connect(sock, (sockaddr*) &add, sizeof(add)) < 0) {
		std::cout << "failed to connect to server" << std::endl;
		return -1;
	}

	std::mutex canAccessBuffer;
	std::atomic_bool stop{false};
	std::map<std::string, section*> sections;
	
	auto f = [&stop, &sock, &sections, &canAccessBuffer]() {
		pollfd fd;
		memset(&fd, 0, sizeof(pollfd));
		fd.fd = sock;
		fd.events = POLLIN;

		std::string curSection;
		bool isCurSectioning = false;
		char charBuf[256];

		while(!stop) {
			if(poll(&fd, 1, 1)) {
				int chars = read(sock, charBuf, 256);
				canAccessBuffer.lock();
				for(int i = 0; i < chars; i++) {
					if(charBuf[i] == (char) 30) {
						isCurSectioning = !isCurSectioning;
						if(isCurSectioning) curSection = "";
						else if(!sections.contains(curSection)) {
							section* s = new section();
							s->name = curSection;
							sections.emplace(std::pair(curSection, s));
						}
					}
					else if(isCurSectioning) curSection.push_back(charBuf[i]);
					else if(!curSection.empty()) {
						if(!sections.contains(curSection)) {
							section* s = new section();
							s->name = curSection;
							sections.emplace(std::pair(curSection, s));
						}
						sections[curSection]->buffer.push_back(charBuf[i]);
					}
				}
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


	coloringFunctions.emplace('E', []() {
			ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4(1, 0, 0, 1);
			});
	coloringFunctions.emplace('N', []() {});
	coloringFunctions.emplace('R', []() {
			ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4(1, 1, 1, 1);
			});
	coloringFunctions.emplace('T', []() {
			ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4(1, 1, 0, 1);
			});
	coloringFunctions.emplace('H', []() {
			ImGui::GetStyle().Colors[ImGuiCol_Text] = ImVec4(0.1, 0.1, 1, 1);
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
		
		canAccessBuffer.lock();

		for(std::pair<std::string, section*> pair : sections) {
			drawSection(*pair.second);
		}

		for(std::string s : toDelete) {
			section* sect = sections[s];
			sections.erase(s);
			delete sect;
		}

		canAccessBuffer.unlock();

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

	for(std::pair<std::string, section*> pair : sections) delete pair.second;
}



void drawSection(const section& sect) {
	if(ImGui::CollapsingHeader(sect.name.c_str())) {
		ImGui::Checkbox("Scroll to bottom", &sect.mouse);
		ImGui::SameLine();
		if(ImGui::Button("Delete Section")) toDelete.push_back(sect.name);

		if(ImGui::BeginChild("scrolling", ImVec2(0, 150), false, ImGuiWindowFlags_HorizontalScrollbar)) {
			if(!sect.buffer.empty()) {
				for(size_t offset = 0; offset < sect.buffer.size(); ) {
					size_t end = 0;
					bool didbreak = false;

					while(offset + end < sect.buffer.size()) {
						if(sect.buffer[offset + end] == '\n') {
							ImGui::TextUnformatted(&sect.buffer.front() + offset, &sect.buffer.front() + offset + end);
							didbreak = true;
							break;
						}

						else if(sect.buffer[offset + end] == (char) 29) {
							ImGui::TextUnformatted(&sect.buffer.front() + offset, &sect.buffer.front() + offset + end);
							coloringFunctions[sect.buffer[offset + end + 1]]();
							ImGui::SameLine(0, 0);
							didbreak = true;
							end++;
							break;
						}

						end++;
					}

					if(!didbreak) {
						ImGui::TextUnformatted(&sect.buffer.front() + offset, &sect.buffer.front() + offset + end);
					}

					offset += end + 1;
				}
				ImGui::StyleColorsDark();
			}

			if(ImGui::GetIO().MouseWheel != 0) sect.mouse = false;
			if(sect.mouse) ImGui::SetScrollHereY(1.0f);

		}
		ImGui::EndChild();
	}
}
