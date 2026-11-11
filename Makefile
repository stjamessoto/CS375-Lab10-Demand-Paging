# CS375 Lab 10 - Demand Paging + CPU Scheduling Simulator
#
#   make              build bin/vmsim
#   make run          run all four policies on input/trace.txt
#   make traces       regenerate the access-pattern traces in input/
#   make experiments  run every pattern / policy / TLB size, fill output/
#   make plots        draw the timeline + comparison graphs (output/graphs)
#   make report       build report/report.pdf
#   make submission   collect everything to hand in into submission/
#   make clean        remove build files and generated output

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2
SRC_DIR  := src
BIN      := bin/vmsim
SRCS     := $(wildcard $(SRC_DIR)/*.cpp)
OBJS     := $(patsubst $(SRC_DIR)/%.cpp,build/%.o,$(SRCS))
PYTHON   ?= python3

.PHONY: all run traces experiments plots report submission clean

all: $(BIN)

$(BIN): $(OBJS)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $@ $^

build/%.o: $(SRC_DIR)/%.cpp $(wildcard $(SRC_DIR)/*.h)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(BIN)
	./$(BIN)

traces:
	$(PYTHON) scripts/gen_traces.py

experiments: $(BIN)
	bash scripts/run_experiments.sh

plots:
	$(PYTHON) scripts/plotter.py

report:
	cd report && pdflatex -interaction=nonstopmode -halt-on-error report.tex >/dev/null && \
	             pdflatex -interaction=nonstopmode -halt-on-error report.tex >/dev/null && \
	             echo "Built report/report.pdf"

submission:
	bash scripts/make_submission.sh

clean:
	rm -rf build bin output/logs output/events output/summary output/graphs \
	       report/*.aux report/*.log report/*.out report/*.toc
