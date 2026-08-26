# Builds the real billybass/voice.cpp against a stub Arduino.h and runs it on
# synthetic signals. The board has no way to report why the gate did or did not
# open on a passing sound, so the tuning happens here first.
#
#   make && ./voicetest

CXXFLAGS = -std=c++14 -O2 -Wall -Wextra -I. -I../../billybass

voicetest: test.cpp ../../billybass/voice.cpp ../../billybass/voice.h Arduino.h
	$(CXX) $(CXXFLAGS) ../../billybass/voice.cpp test.cpp -o $@

clean:
	rm -f voicetest

.PHONY: clean
