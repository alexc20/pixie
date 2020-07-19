#!/bin/bash

# We use GCC for building coverage code.
export CC=gcc
export CXX=g++

set -e

# We can consider adding this file to our repo if needed.
CODECOV_VERSION="db834f033a450a75a315e72ae8490393c9b19181"
CODECOV_SCRIPT="https://raw.githubusercontent.com/codecov/codecov-bash/${CODECOV_VERSION}/codecov"

GIT_COMMIT=""
GIT_BRANCH=""
CODECOV_TOKEN=""
GENERATE_HTML=false
UPLOAD_TO_CODECOV=false
HTML_OUTPUT_DIR=""

CC_COVERAGE_FILE="cc_coverage.info"
GO_COVERAGE_FILE="coverage.txt"
UI_OUTPUTS=bazel-testlogs/src/ui/ui-tests/test.outputs/outputs.zip

# Print out the usage information and exit.
usage() {
  echo "Usage $0 [-u] [-g] [-t <codecov_token>] [-c <git_commit>] [-b <git_branch>] [-o <output_dir>]" 1>&2;
  echo "   -u    Upload to CodeCov. Requires -t, -c, -b"
  echo "   -g    Generate LCOV html. Requires -o"
  exit 1;
}

# Print out the config information.
print_config() {
  echo "Config: "
  echo "  Upload to CodeCov: ${UPLOAD_TO_CODECOV}"
  echo "  GIT_COMMIT       : ${GIT_COMMIT}"
  echo "  GIT_BRANCH       : ${GIT_BRANCH}"
  echo "  CODECOV_TOKEN    : ${CODECOV_TOKEN}"
  echo "  Generate HTML    : ${GENERATE_HTML}"
  echo "  HTML_OUTPUT_DIR  : ${HTML_OUTPUT_DIR}"
}

check_config() {
  if [ "${UPLOAD_TO_CODECOV}" = true ]; then
    if [ "${GIT_COMMIT}" = "" ]; then
      echo "Option -c to specify git commit is required wih -u"
      exit 1
    fi
    if [ "${GIT_BRANCH}" = "" ]; then
      echo "Option -b to specify git branch is required wih -u"
      exit 1
    fi
    if [ "${CODECOV_TOKEN}" = "" ]; then
      echo "Option -t to specify codecov token is required wih -u"
      exit 1
    fi
  fi

  if [ "${GENERATE_HTML}" = true ]; then
    if [ "${HTML_OUTPUT_DIR}" = "" ]; then
      echo "Option -p to specify lcov output is required wih -g"
      exit 1
    fi
  fi
}

parse_args() {
  local OPTIND
  # Process the command line arguments.
  while getopts "guc:b:t:o:h" opt; do
    case ${opt} in
      g)
        GENERATE_HTML=true
        ;;
      u)
        UPLOAD_TO_CODECOV=true
        ;;
      c)
        GIT_COMMIT=$OPTARG
        ;;
      b)
        GIT_BRANCH=$OPTARG
        ;;
      t)
        CODECOV_TOKEN=$OPTARG
        ;;
      o)
        HTML_OUTPUT_DIR=$OPTARG
        ;;
      :)
        echo "Invalid option: $OPTARG requires an argument" 1>&2
        ;;
      h)
        usage
        ;;
      *)
        usage
        ;;
    esac
  done
  shift $((OPTIND -1))
}

generate_html() {
  genhtml -o ${HTML_OUTPUT_DIR} -s ${CC_COVERAGE_FILE}

  echo "****************************************************"
  echo "* For Go HTML do the following:                     "
  echo "*     go tool cover -html=${GO_COVERAGE_FILE}       "
  echo "****************************************************"
}

upload_to_codecov() {
  bash <(curl -s ${CODECOV_SCRIPT}) -t ${CODECOV_TOKEN} -B ${GIT_BRANCH} -C ${GIT_COMMIT}
}

# We use globs, make sure they are supported.
shopt -s globstar

# Parse the input arguments.
parse_args "$@"

# Check config parameters.
check_config

# Print config parameters.
print_config

cd $(bazel info workspace)

# Get coverage from bazel targets.
bazel coverage --cache_test_results=no //src/...

# This finds all the valid coverage files and then creates a list of them
# prefixed by -a, which allows up to add them to the lcov output.
# This part only works for C++ coverage.
file_merge_args=""
for file in bazel-out/**/coverage.dat
do
    # Only consider valid files. Some files only contain Go coverage and that
    # does not work with LCOV.
    lcov --summary ${file} 2>&1 >/dev/null && file_merge_args+=" -a ${file}"
done

# Merge all the files.
lcov $file_merge_args -o cc_coverage.info

# Print out the summary.
lcov --summary ${CC_COVERAGE_FILE}

# Remove test files from the coverage files.
lcov -r ${CC_COVERAGE_FILE} '**/*_test.cc' -o ${CC_COVERAGE_FILE}
lcov -r ${CC_COVERAGE_FILE} '**/*_mock.cc' -o ${CC_COVERAGE_FILE}
lcov -r ${CC_COVERAGE_FILE} '**/*_mock.h' -o ${CC_COVERAGE_FILE}

# Print out the final summary.
lcov --summary ${CC_COVERAGE_FILE}

# Create go coverage file, by grabbing all the .go entries.
echo "mode: set" > coverage.tmp
for file in bazel-out/**/coverage.dat
do
    grep ".go" ${file} >> coverage.tmp || true
done

# Remove test files from the go coverage.
grep -v "_test.go" coverage.tmp > ${GO_COVERAGE_FILE}
rm -f coverage.tmp

# Extract the UI coverage files. They are part of the zip archive.
unzip -o "${UI_OUTPUTS}"

# Upload to codecov.io.
if [ "${UPLOAD_TO_CODECOV}" = true ]; then
  upload_to_codecov
fi

# Generate HTML.
if [ "${GENERATE_HTML}" = true ]; then
  generate_html
fi
