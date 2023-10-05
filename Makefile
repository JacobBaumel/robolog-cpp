EXE = robolog

all: compile
	./$(EXE)

compile: main.cpp
	g++ -std=c++20 main.cpp imgui.a -I ../glfw/include -I ../imgui -I ../imgui/backends -L ./ -lglfw -lGL -o $(EXE) -g
