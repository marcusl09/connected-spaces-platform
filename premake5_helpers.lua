#!lua

if not CSP then
	CSP = {}

    function CSP.IsRunningNightlyBuild()
		-- Temporary measure to prevent CI from running tests due to UploadAssetTest hanging when run
        return os.getenv("NIGHTLY_BUILD") ~= nil
    end
    
    function CSP.IsRunningOnTeamCityAgent()
		-- Temporary measure to prevent CI from running tests due to UploadAssetTest hanging when run
        return os.getenv("TEAMCITY_VERSION") ~= nil
    end

    function CSP.UseStandardSettings()
        -- Build location
        targetdir "%{prj.location}/Binaries/%{cfg.platform}/%{cfg.buildcfg}"

        -- Intermediate C++ files
        objdir "%{prj.location}/Intermediate"
        
        -- Config for platforms
        filter "platforms:x64"
            system "Windows"
            systemversion "10.0.17763.0"
            architecture "x86_64"
        filter "platforms:Android"
            system "Android"
            architecture("ARM64")
            stl("libc++")
            exceptionhandling ("On")
        filter "platforms:macosx"
            system "macosx"
            systemversion "10.15"
            architecture "x86_64"
        filter "platforms:ios"
            system "ios"
            systemversion "15.0"
            architecture "x86_64"
		filter "platforms:wasm"
            system "linux"
            architecture "x86"
        filter {}
        
        -- C++
        language "C++"
        cppdialect "C++17"
        
        -- Standard debug/release config settings
        filter "configurations:*Debug*"
            defines { "DEBUG" }
            symbols "Full"
            optimize "Off"
        filter "configurations:*Release*"
            defines { "NDEBUG" }
            symbols "Off"
            optimize "On"

            xcodebuildsettings {
                -- Required to ACTUALLY make xcode not generate symbols as `symbols` is broken
                ["GCC_GENERATE_DEBUGGING_SYMBOLS"] = "NO",
                -- Enable LTO to trim some fat off of our Apple builds
                ["LLVM_LTO"] = "YES"
            }
        filter {}
        
        -- Static/shared lib settings
        filter "configurations:*Static*"
            staticruntime "On"
        filter "configurations:*DLL*"    
            staticruntime "Off"
        filter {}        
        
        -- System defines
        filter "system:Windows"
            defines {
                "NOMINMAX",
                "_WINDOWS",
                "CSP_WINDOWS"
            }

			--[[
                We always use the release version of MS runtime DLLS to ensure that we can
			    run foundation debug binaries on non-developer machines
            ]]--
        	runtime "Release"
        filter "system:Android"
            defines {
                "CSP_ANDROID"
            }
        filter "system:macosx"
            defines {
                "CSP_MACOSX"
            }
        filter "system:ios"
            defines {
                "CSP_IOS"
            }
        filter "platforms:wasm"
			defines {
                "CSP_WASM"
            }
        filter {}
    end
    
    if not CSP.Platforms then
        CSP.Platforms = {}
        
        function CSP.Platforms.AddWindows()
            if not os.istarget("macosx") and not os.istarget("ios") then -- If on windows
                platforms { "x64" }
                systemversion "10.0.17763.0"
            end
        end
        
        function CSP.Platforms.AddAndroid()
            if not os.istarget("macosx") and not os.istarget("ios") then -- If on windows
                platforms { "Android" }
                androidapilevel(30)
            end
        end
        
        function CSP.Platforms.AddMac()
            if os.istarget("macosx") then
                platforms { "macosx" }
            end
        end
        
        function CSP.Platforms.AddIOS()
            if os.istarget("ios") then
                platforms { "ios" }
            end
        end
		
		function CSP.Platforms.AddWebAssembly()
            if CSP.IsWebAssemblyGeneration() then
                platforms { "wasm" }
            end
        end
    end

    function CSP.IsWebAssemblyGeneration()
        if(_OPTIONS["generate_wasm"] ~= nil and _ACTION ~= "gmake2") then
            error("Unsupported project generation with the combination of action: " .. _ACTION .. " and option --generate_wasm. Please check the --generate_wasm option help.")
        end 

        return (_OPTIONS["generate_wasm"] ~= nil)
    end

    function CSP.IsTargettingMacOS()
        return os.istarget("macosx") or os.istarget("ios")
    end

    function CSP.IsGeneratingVS()
        return _ACTION:find("vs20", 1, #"vs20")
    end

    function CSP.IsGeneratingXCode()
        return _ACTION == "xcode4"
    end

    function CSP.IsGeneratingCPPOnMac()
        return CSP.IsTargettingMacOS() and CSP.IsGeneratingXCode()
    end

    function CSP.IsGeneratingCSharpOnMac()
        return CSP.IsTargettingMacOS() and CSP.IsGeneratingVS()
    end
    
    function CSP.HasCommandLineArgument(Argument)
        HasArgument = false        
        for i, arg in ipairs(_ARGS) do
            if arg == Argument then
                HasArgument = true
            end
        end        
        return HasArgument
    end
end

return CSP
