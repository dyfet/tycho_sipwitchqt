@rem
@set "bootstrap=%cd%"
@set "output=%1"
@set "openssl=%3"
@set "arch=%4"
@cd %output%
@mkdir Bootstrap
@cd Bootstrap

@cmake "%bootstrap%" "-DPORTS_BUILD_TYPE=%build%" "-DPORTS_ARCH=%arch%" "-DOPENSSL_PREFIX=%openssl%" "-GNMake Makefiles"
@cmake --build . --config release
