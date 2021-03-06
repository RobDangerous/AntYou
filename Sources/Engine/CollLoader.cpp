#include "pch.h"
#include "CollLoader.h"
#include <Kore/IO/FileReader.h>
#include <cstring>
#include <cstdlib>

using namespace Kore;

namespace {
	char* tokenize(char* s, char delimiter, int& i) {
		int lastIndex = i;
		char* index = strchr(s + lastIndex + 1, delimiter);
		if (index == nullptr) {
			return nullptr;
		}
		int newIndex = (int)(index - s);
		i = newIndex;
		int length = newIndex - lastIndex;
		char* token = new char[length + 1];
		strncpy(token, s + lastIndex + 1, length);
		token[length] = 0;
		return token;
	}

	vec3 parseVertex(char* line) {
		float x = (float)strtod(strtok(nullptr, " "), nullptr);
		float y = (float)strtod(strtok(nullptr, " "), nullptr);
		float z = (float)strtod(strtok(nullptr, " "), nullptr);

		return vec3(x, y, z);
	}

	bool parseLine(char* line, vec3 &v, bool &isSet) {
		char* token = strtok(line, " ");
		if (strcmp(token, "v") == 0) {
			v = parseVertex(line);
			isSet = true;
			return false;
		}
		else if (strcmp(token, "s") == 0) {
			return true;
		}
		return false;
	}
}

void loadColl(const char* filename, vec4 &min, vec4 &max, int &index) {
	FileReader fileReader(filename, FileReader::Asset);
	void* data = fileReader.readAll();
	int length = fileReader.size() + 1;
	char* source = new char[length + 1];
	for (int i = 0; i < length; ++i) source[i] = reinterpret_cast<char*>(data)[i];
	source[length] = 0;
	
	vec3 v;
	bool isSet = false;
	char* line = tokenize(source, '\n', index);
	while (line != nullptr) {
		if (parseLine(line, v, isSet)) {
			return;
		}
		else {
			if (isSet) {
				min.set(Kore::min(min.x(), v.x()),
					Kore::min(min.y(), v.y()),
					Kore::min(min.z(), v.z()));

				max.set(Kore::max(max.x(), v.x()),
					Kore::max(max.y(), v.y()),
					Kore::max(max.z(), v.z()));
				isSet = false;
			}
		}

		delete[] line;
		line = tokenize(source, '\n', index);
	}

	index = -1;
	return;
}
