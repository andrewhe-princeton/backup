UTIL_UNITS=empty_template helpers control_flow_equivalence dominator_summary
ENABLER_UNITS=loop_invariant_code_motion
ANALYSIS_UNITS=dependence_graphs iv_attributes sccdag_attributes loop_domain_space
ALL_UNITS=$(UTIL_UNITS) $(ENABLER_UNITS) $(ANALYSIS_UNITS)

all: setup $(ALL_UNITS)

run:
	for i in `ls`; do ../scripts/run_unit.sh $$i ; done

setup:
	mkdir -p `realpath ../../install`/test

control_flow_equivalence:
	cd $@ ; NOELLE_INSTALL_DIR=`realpath ../../../install`/test ../../../src/scripts/run_me.sh

dependence_graphs:
	cd $@ ; NOELLE_INSTALL_DIR=`realpath ../../../install`/test ../../../src/scripts/run_me.sh

dominator_summary:
	cd $@ ; NOELLE_INSTALL_DIR=`realpath ../../../install`/test ../../../src/scripts/run_me.sh

empty_template:
	cd $@ ; NOELLE_INSTALL_DIR=`realpath ../../../install`/test ../../../src/scripts/run_me.sh

helpers:
	cd $@ ; NOELLE_INSTALL_DIR=`realpath ../../../install`/test ../../../src/scripts/run_me.sh

iv_attributes:
	cd $@ ; NOELLE_INSTALL_DIR=`realpath ../../../install`/test ../../../src/scripts/run_me.sh

loop_domain_space:
	cd $@ ; NOELLE_INSTALL_DIR=`realpath ../../../install`/test ../../../src/scripts/run_me.sh

loop_invariant_code_motion:
	cd $@ ; NOELLE_INSTALL_DIR=`realpath ../../../install`/test ../../../src/scripts/run_me.sh

sccdag_attributes:
	cd $@ ; NOELLE_INSTALL_DIR=`realpath ../../../install`/test ../../../src/scripts/run_me.sh

clean:
	rm -f *.txt ;
	rm -rf */build ;
	find ./ -name compile_commands.json -delete
	find ./ -name *.bc -delete
	find ./ -name *.ll -delete
	find ./ -name output.prof -delete
	find ./ -name default.profraw -delete
	find ./ -name compiler_output.txt -delete
	find ./ -name test_output.txt -delete
	find ./ -name test_pre_prof -delete
	find ./ -name .ycm_extra_conf.py -delete ;
	find ./ -name compile_commands.json -delete ;
	find ./ -name .clangd -exec rm -rv {} +

.PHONY: $(ALL_UNITS)

.PHONY: clean setup run
