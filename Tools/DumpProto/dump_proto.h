#pragma once

//#include "../../libthecore/include/stdafx.h"
//#include ""
//#define __WIN32__
#include <stdio.h>
#include <string>
#include <map>
#include <set>
#include <algorithm>

#include "lzo.h"

//������exe���� ����鼭 ���� �߰� : ���� �о�� �� �ֵ��� �Ͽ���.
#include "CsvFile.h"
#include "ItemCSVReader.h"

#ifdef _DEBUG
#pragma comment(lib, "lzo2_debug.lib")
#else
#pragma comment(lib, "lzo2.lib")
#endif