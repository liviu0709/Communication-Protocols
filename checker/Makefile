# Hide admin credentials from the Makefile
ifneq (,$(wildcard .env))
include .env
export
endif
# Python + Flask makefile

VENV = .venv
VENV_PYTHON3 = $(VENV)/bin/python3

ADMIN ?= default_admin_value
PROGRAM ?= ../client

all: venv deps

venv: $(VENV_PYTHON3)
$(VENV_PYTHON3):
	python3 -m venv "$(VENV)"

deps: venv
	$(VENV_PYTHON3) -m pip install -r requirements.txt

A ?= --debug --admin "$(ADMIN)"
run: client
	$(VENV_PYTHON3) checker.py $(PROGRAM) $(A)

client: ../client.cpp
	g++ -o ../client ../client.cpp
