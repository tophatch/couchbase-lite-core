//
// PublicKey+Windows.cc
//
// Copyright © 2020 Couchbase. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma comment(lib, "ncrypt")

#include "Certificate.hh"
#include "PublicKey.hh"
#include "TLSContext.hh"
#include "Logging.hh"
#include "Error.hh"
#include "ParseDate.hh"
#include "StringUtil.hh"
#include "mbedUtils.hh"
#include "mbedtls/pk.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/md5.h"
#include "TempArray.hh"
#include "Defer.hh"
#include <Windows.h>
#include <ncrypt.h>
#include <functional>
#include <atomic>
#include <codecvt>
#include <chrono>
