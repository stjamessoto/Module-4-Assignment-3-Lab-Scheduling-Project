# Builds everything into build/ so the submission/ folder only ever contains source files.
#   make          build the 10 lab programs and the simulator
#   make report   build report/report.pdf (after adding screenshots)
#   make clean    delete build/ and LaTeX temporary files
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
LAB      := fcfs sjf srtf priority rr mlq mlfq lottery cfs edf
SIM_DIR  := submission/Part3_Simulator

all: lab sim

lab: $(addprefix build/,$(LAB))

sim: build/simulator

build/%: submission/Part2_Lab/%.cpp | build
	$(CXX) $(CXXFLAGS) $< -o $@

build/simulator: $(SIM_DIR)/simulator.cpp $(wildcard $(SIM_DIR)/*.h) | build
	$(CXX) $(CXXFLAGS) -pthread $< -o $@

build:
	mkdir -p build

report:
	cd report && pdflatex -interaction=nonstopmode -halt-on-error report.tex >/dev/null && pdflatex -interaction=nonstopmode -halt-on-error report.tex >/dev/null
	@echo "Built report/report.pdf"

clean:
	rm -rf build
	rm -f report/*.aux report/*.log report/*.out report/*.toc

.PHONY: all lab sim report clean
