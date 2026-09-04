.PHONY: configure build test verify clean benchmark

BUILD_DIR ?= build

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release

build: configure
	cmake --build $(BUILD_DIR) --parallel

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure
	python3 -m unittest discover -s tests -p 'test_*.py' -v

verify:
	bash scripts/verify.sh

benchmark: build
	python3 scripts/benchmark.py --binary $(BUILD_DIR)/marketreplay --events 500000 --output evidence/benchmark.json --markdown evidence/BENCHMARK.md

clean:
	rm -rf $(BUILD_DIR) evidence/generated evidence/verification
