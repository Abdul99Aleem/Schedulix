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

ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

# Include project level config
include $(ROOT_DIR)/../common.mk

NAME=mcp2515

EXTRA_INCVPATH += $(PROJECT_ROOT)/public
LIBS += can slog2

define PINFO
PINFO DESCRIPTION=MCP2515 CAN Controller Library
endef

include $(MKFILES_ROOT)/qtargets.mk
