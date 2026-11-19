.PHONY: configure build test verify clean benchmark sanitize

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

sanitize:
	cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DMARKETREPLAY_ENABLE_SANITIZERS=ON
	cmake --build build-sanitize --parallel
	./build-sanitize/marketreplay_tests
	python3 scripts/differential.py --binary build-sanitize/marketreplay --scenarios 18 --output build-sanitize/differential.json
	python3 scripts/robustness.py --binary build-sanitize/marketreplay --mutations 128 --output build-sanitize/robustness.json

# evidence/verification is committed release evidence, so clean only removes build output.
clean:
	rm -rf $(BUILD_DIR) build-sanitize evidence/generated
