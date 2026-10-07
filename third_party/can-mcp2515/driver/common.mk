# Copyright (c) 2025, BlackBerry Limited. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
ROOT_DIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))

# Figure out if I'm running from within a git repo. If I am
# I'll encode the status of the repo into the image.
# IS_GIT_REPO==0 if it is a git repo, anything else is not a repo
IS_GIT_REPO=$(shell git rev-parse --git-dir 2>/dev/null 1>&2; echo $$?)

#Define the version
CAN_MCP2515_VERSION_MAJOR=1
CAN_MCP2515_VERSION_MINOR=0
CAN_MCP2515_VERSION_PATCH=0
CAN_MCP2515_VERSION_STRING=$(CAN_MCP2515_VERSION_MAJOR).$(CAN_MCP2515_VERSION_MINOR).$(CAN_MCP2515_VERSION_PATCH)

ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

# Include project level config
include $(ROOT_DIR)/../common.mk

NAME=can-mcp2515

LIBS += mcp2515$(VARIANT_TAG) can secpol slog2

define PINFO
PINFO DESCRIPTION=MCP2515 CAN Controller ResMgr
PINFO VERSION=$(CAN_MCP2515_VERSION_STRING)
PINFO REPO_STATUS=$(CAN_MCP2515_REPO_STATUS)
endef

# If I'm in a git repo, generate the repo status information
# before building anything else.
# Setting PRE_TARGET has to happen before including qtargets.mk
ifeq ($(IS_GIT_REPO),0)
PRE_TARGET += repo_status
endif
POST_CLEAN += rm -f repo_status.txt

# Add version/repo-status when building version.o
version.o : CPPFLAGS += -DCAN_MCP2515_VERSION_MAJOR=$(CAN_MCP2515_VERSION_MAJOR) \
                        -DCAN_MCP2515_VERSION_MINOR=$(CAN_MCP2515_VERSION_MINOR) \
                        -DCAN_MCP2515_VERSION_PATCH=$(CAN_MCP2515_VERSION_PATCH) \
	                -DCAN_MCP2515_REPO_STATUS="\"$(CAN_MCP2515_REPO_STATUS)\""

include $(MKFILES_ROOT)/qtargets.mk

# Logic to figure out the status of the GIT repo.
# If I'm not in a git repo, just set the variable to be an empty string.
ifeq ($(IS_GIT_REPO),0)
.PHONY: repo_status
repo_status:
	@git log -1 --pretty=format:"%h" >> new_repo_status.txt
	@if [ `git status --porcelain=1 --untracked-files=no | wc -l` -ne 0 ]; then echo -n '*' >> new_repo_status.txt; fi
	@if ! diff -q -N new_repo_status.txt repo_status.txt 2>/dev/null 1>&2; then \
		echo "Updating repo status"; \
		cp new_repo_status.txt repo_status.txt; \
	fi
	@rm new_repo_status.txt
repo_status.txt : repo_status
version.o : repo_status.txt
CAN_MCP2515_REPO_STATUS=$(file < repo_status.txt)
else
CAN_MCP2515_REPO_STATUS=""
endif
clean: 

