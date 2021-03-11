SHELL=/bin/bash
.DEFAULT_GOAL := help

# https://gist.github.com/tadashi-aikawa/da73d277a3c1ec6767ed48d1335900f3
.PHONY: $(shell grep --no-filename -E '^[a-zA-Z0-9_-]+:' $(MAKEFILE_LIST) | sed 's/://')

BASE_PATH = .
BINARY_PATH = $(BASE_PATH)/bin/main

define gcc
	docker run --rm -w /work -v $(PWD):/work gcc gcc -Wall -O2 -o ./bin/main ${1}
endef

define exec
	docker run --rm -w /work -v $(PWD):/work gcc $(BINARY_PATH) ${1}
endef

# Phony Targets

clean: ## Clean
	rm -f $(BINARY_PATH)

run: ## docker run
	docker run -it --rm --cap-add=SYS_PTRACE --security-opt="seccomp=unconfined" -w /work -v $(PWD):/work gcc /bin/bash || true

cat: ## run cat
	$(call gcc,chap05/cat.c)
	$(call exec,README.md .gitignore)

# https://postd.cc/auto-documented-makefile/
help: ## Show help
	@grep --no-filename -E '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-40s\033[0m %s\n", $$1, $$2}'
