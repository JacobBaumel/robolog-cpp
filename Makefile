EXE = robolog

all: compile
	./$(EXE)

compile: main.cpp
	g++ -std=c++20 main.cpp imgui.a -I ../imgui -I ../imgui/backends -lglfw -lGL -o $(EXE) -g
