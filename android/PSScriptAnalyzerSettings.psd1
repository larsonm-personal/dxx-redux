# PSScriptAnalyzerSettings.psd1 -- Settings for Invoke-ScriptAnalyzer.
# Used by run-psscriptanalyzer.ps1 for both lint and format passes.
@{
    ExcludeRules = @(
        # Intentional console output in build/test scripts
        'PSAvoidUsingWriteHost'
        # Project uses custom verb names (Adb-Timeout, etc.)
        'PSUseApprovedVerbs'
        # Not interactive -- no ShouldProcess needed
        'PSUseShouldProcessForStateChangingFunctions'
        # Positional parameters are fine for simple wrappers
        'PSAvoidUsingPositionalParameters'
        # env: drive is used intentionally
        'PSUseDeclaredVarsMoreThanAssignments'
        # Empty catch blocks are intentional in test cleanup code
        'PSAvoidUsingEmptyCatchBlock'
        # False positives on parameter names containing "password" etc.
        'PSAvoidUsingPlainTextForPassword'
        # Parameters used via splatting or cross-scope; false positives
        'PSReviewUnusedParameter'
        # UTF-8 without BOM per .editorconfig; BOM is unwanted
        'PSUseBOMForUnicodeEncodedFile'
        # Custom noun conventions are fine for utility scripts
        'PSUseSingularNouns'
        # False positives with scriptblocks passed to Start-Job etc.
        'PSUseUsingScopeModifierInNewRunspaces'
        # SHA1 used for content verification, not security
        'PSAvoidUsingBrokenHashAlgorithms'
        # $sender is conventional in .NET event handlers
        'PSAvoidAssignmentToAutomaticVariable'
    )
    Rules = @{
        PSPlaceOpenBrace = @{
            Enable = $true
            OnSameLine = $true
            NewLineAfter = $true
            IgnoreOneLineBlock = $true
        }
        PSPlaceCloseBrace = @{
            Enable = $true
            NoEmptyLineBefore = $false
            IgnoreOneLineBlock = $true
            NewLineAfter = $false
        }
        PSUseConsistentIndentation = @{
            Enable = $true
            IndentationSize = 4
            PipelineIndentation = 'IncreaseIndentationForFirstPipeline'
            Kind = 'space'
        }
        PSUseConsistentWhitespace = @{
            Enable = $true
            CheckInnerBrace = $true
            CheckOpenBrace = $true
            CheckOpenParen = $true
            CheckOperator = $true
            CheckPipe = $true
            CheckPipeForRedundantWhitespace = $false
            CheckSeparator = $true
            IgnoreAssignmentOperatorInsideHashTable = $true
        }
        PSUseCompatibleSyntax = @{
            Enable = $true
            TargetVersions = @('5.1')
        }
        PSUseCompatibleCommands = @{
            Enable = $true
            TargetProfiles = @('win-48_x64_10.0.17763.0_5.1.17763.316_x64_4.0.30319.42000_framework')
        }
        PSUseCompatibleTypes = @{
            Enable = $true
            TargetProfiles = @('win-48_x64_10.0.17763.0_5.1.17763.316_x64_4.0.30319.42000_framework')
        }
    }
}
