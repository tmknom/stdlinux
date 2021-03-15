SHELL=/bin/bash
.DEFAULT_GOAL := help

# https://gist.github.com/tadashi-aikawa/da73d277a3c1ec6767ed48d1335900f3
.PHONY: $(shell grep --no-filename -E '^[a-zA-Z0-9_-]+:' $(MAKEFILE_LIST) | sed 's/://')

BASE_PATH = .
BINARY_PATH = $(BASE_PATH)/bin/main

define gcc
	docker run --rm -w /work -v $(PWD):/work debian:gcc gcc -Wall -O2 -o ./bin/main ${1}
endef

define exec
	docker run --rm -w /work -v $(PWD):/work debian:gcc $(BINARY_PATH) ${1}
endef

define debug
	docker run --rm -w /work -v $(PWD):/work debian:gcc gcc -Wall -g -o ./bin/main ${1}
	docker run -it --rm --cap-add=SYS_PTRACE --security-opt="seccomp=unconfined" -w /work -v $(PWD):/work debian:gcc /usr/bin/gdb ./bin/main
endef

# Phony Targets

clean: ## Clean
	rm -f $(BINARY_PATH)

run: ## docker run
	docker run -it --rm --cap-add=SYS_PTRACE --security-opt="seccomp=unconfined" -w /work -v $(PWD):/work debian:gcc /bin/bash || true

cat: ## run cat
	$(call gcc,chap05/cat.c)
	$(call exec,README.md .gitignore)

cat2: ## run cat2
	$(call gcc,chap06/cat2.c)
	$(call exec,README.md .gitignore)

head: ## run head
	$(call gcc,chap06/head.c)
	$(call exec,-n 10 chap06/head.c)

head-debug: ## debug head
	$(call debug,chap06/head.c)

grep: ## run grep
	$(call gcc,chap08/grep.c)
	$(call exec,reg chap08/grep.c)

ls: ## run ls
	$(call gcc,chap10/ls.c)
	$(call exec,.)

mkdir: ## run mkdir
	$(call gcc,chap10/mkdir.c)
	$(call exec,chap10/tmp)

rmdir: ## run rmdir
	$(call gcc,chap10/rmdir.c)
	$(call exec,chap10/tmp)

ln: ## run ln
	$(call gcc,chap10/ln.c)
	$(call exec,chap10/ln.c chap10/another)

symlink: ## run symlink
	$(call gcc,chap10/symlink.c)
	$(call exec,chap10/symlink.c chap10/another)

rm: ## run rm
	$(call gcc,chap10/rm.c)
	$(call exec,chap10/another)

mv: ## run mv
	$(call gcc,chap10/mv.c)
	date > chap10/org
	$(call exec,chap10/org chap10/another)

stat: ## run stat
	$(call gcc,chap10/stat.c)
	$(call exec,chap10/stat.c)

chmod: ## run chmod
	$(call gcc,chap10/chmod.c)
	$(call exec,777 chap10/chmod.c)

spawn: ## run spawn
	$(call gcc,chap12/spawn.c)
	$(call exec,/bin/echo OK)

env: ## run env
	$(call gcc,chap14/env.c)
	$(call exec)

server: ## run server
	docker run --rm -w /work -v $(PWD):/work debian:gcc gcc -Wall -O2 -c -o ./bin/main.o chap16/server.c

# https://postd.cc/auto-documented-makefile/
help: ## Show help
	@grep --no-filename -E '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-40s\033[0m %s\n", $$1, $$2}'
