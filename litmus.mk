# for `run` target allow -- as separator
# e.g. make run -- arg0 arg1 arg2
# to be equivalent to make run BIN_ARGS="arg0 arg1 arg2"
ifeq ($(word 1,$(MAKECMDGOALS)),run)
  define leading
    $(if $(findstring run,$(firstword $1)),sep $(firstword $1),\
      $(firstword $1) $(call leading, $(wordlist 2,$(words $1),$1)))
  endef
  # assume format of command is `make [opt...] run -- cmds`
  BIN_ARGS = $(wordlist $(words $(call leading,$(MAKECMDGOALS))),$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  # make dummy targets for BIN_ARGS so Make doesn't try run them
  # see https://stackoverflow.com/questions/2214575/passing-arguments-to-make-run
  $(eval $(BIN_ARGS):;@:)
else
  BIN_ARGS =
endif

# can pass LITMUS_FILES manually to supply a space-separated list of all the .c files to compile and link in the litmus/ directory
# otherwise the Makefile will try auto-discover the litmus/*.c files and the litmus/litmus_tests/ files
#	(according to either the supplied LITMUS_TESTS or checking against previous runs)
# or if TEST_DISCOVER=0  then just try compile *all* .c files inside litmus/ and litmus/litmus_tests/**/
# (but the user has to manually edit groups.c so the harness itself knows about them)
ifeq ($(strip $(TEST_DISCOVER)),1)
   ifneq ($(findstring litmus,$(MAKECMDGOALS)),)
   ifndef LITMUS_FILES
      # if fail, ignore
      out := $(shell ($(MAKE_TEST_LIST_CMD) $(quiet) $(LITMUS_TESTS) 2>/dev/null) || echo "FAIL")

      # if the script that generates groups.c fails
      # then if there is no groups.c copy one from the backup
      ifeq ($(out),FAIL)
        $(warning \
			Generation of litmus/groups.c failed. \
		 	Expecting user management of groups.c file. \
		)

        outcp := $(shell [ -f litmus/groups.c ] || (echo FAIL; cp litmus/backup/* litmus/))

        ifeq ($(outcp),FAIL)
            $(warning \
				No groups.c file detected. \
		 		Creating fresh ... \
			)
        endif
      endif

   	  litmus_test_list = $(shell awk '$$1==1 {print $$2}' litmus/test_list.txt)
   	  LITMUS_TEST_FILES ?= $(litmus_test_list)
   endif
   endif
else
   LITMUS_TEST_FILES ?= $(wildcard litmus/litmus_tests/**/*.c)
endif

LITMUS_FILES := $(wildcard litmus/*.c) $(LITMUS_TEST_FILES)
litmus_BIN_FILES := $(addprefix bin/,$(LITMUS_FILES:.c=.o))

# Linting

LINTER = python3 litmus/linter.py
NO_LINT = 0

.PHONY: lint
lint:
	@$(LINTER) $(LITMUS_TEST_FILES)

.PHONY: litmus_tests
litmus_tests:
	$(call run_cmd,make,Collecting new tests,\
		 $(MAKE_TEST_LIST_CMD) $(quiet) $(LITMUS_TESTS) || { \
			echo "Warning: failed to update groups.c. test listing may be out-of-sync." 1>&2 ; \
			( which python3 &> /dev/null ) && echo "litmus_tests target requires python3." 1>&2 ; \
		};\
	)
	@echo 're-run `make` to compile any new or updated litmus tests'


bin/litmus/%.o: litmus/%.c | check_cross_tools_exist
	$(run_cc)
ifeq ($(quiet),0)
ifeq ($(NO_LINT),0)
	@$(LINTER) $<
endif
endif

# remove bin/lib/version.o and let that build separately
# so any change to any of the other prerequesites will
# force a re-fresh of version.o containing the version string
ELF_PREREQ = $(filter-out bin/lib/version.o, $(COMMON_BIN_FILES)) $(litmus_BIN_FILES)
bin/lib/version.o: $(ELF_PREREQ)

# then add version.o back in so direct changes to it also cause a rebuild
# then make the ELF from all of them
bin/litmus.elf: bin/lib/version.o $(ELF_PREREQ)
	$(call run_cmd,LD,$@,\
		$(LD) $(LDFLAGS) -o $@ -T bin.lds $(COMMON_BIN_FILES) $(litmus_BIN_FILES))
	$(call run_cmd,OBJDUMP,$@.S,\
		$(OBJDUMP) $(OBJDUMPFLAGS) -D $@ > $@.S)
	$(call run_cmd,CP,$@.debug,\
		cp $@ $@.debug)
	$(call run_cmd,STRIP,$@,\
		$(OBJCOPY) -g $@)

bin/litmus.bin: bin/litmus.elf
	$(call run_cmd,OBJCOPY,$@,\
		$(OBJCOPY) -O binary bin/litmus.elf bin/litmus.bin)