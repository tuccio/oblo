param (
    [string] $Root = "$PSScriptRoot/../.build",
    [string] $Compiler = "$env:VULKAN_SDK/Bin/glslc.exe"  
)

# Script that recompiles shaders in the shader cache using the glslc shipped with VulkanSDK
# It searches recursively for shader files, expands the includes and outputs the source file to an .exp.glsl file
# Finally compiles the shader into a spirv with embedded debug information

function ExpandIncludes {
    param (
        [string] $Path,
        [string] $Out
    )
    
    $item = Get-Item $Path
    $includeDir = "$($item.Directory)/../vulkan/shaders"

    $fileContent = (Get-Content $Path)

    while ($true) {
        $hasIncludes = $false

        $newFileContent = New-Object -TypeName "System.Text.StringBuilder";

        $fileContent -split [Environment]::NewLine | ForEach-Object {
            $m = $_ -match "#include <(.*)>$"

            if ($m) {
                $hasIncludes = $true
                $includeFullPath = "$includeDir/$($Matches[1]).glsl"
                Get-Content $includeFullPath | ForEach-Object { $newFileContent.AppendLine($_) | Out-Null }
            }
            else {
                $newFileContent.AppendLine($_) | Out-Null
            }
        }

        $fileContent = $newFileContent

        if (!$hasIncludes) {
            break
        }
    }

    Set-Content -Path $Out -Value $fileContent
}

function CompileShader {
    param (
        [string] $Path
    )

    $item = Get-Item $Path
    $outFile = "$($item.Directory)/$($item.BaseName).spirv"
    
    $debugFlags = "-g"
    $optimizationFlags = "-O0"

    & $Compiler --target-env=vulkan1.3 --target-spv=spv1.5 $optimizationFlags $debugFlags -o $outFile $Path
}

Get-ChildItem -Path $Root -Recurse -ErrorAction SilentlyContinue -Force | ForEach-Object {
    $isShader = @(".vert", ".frag", ".comp", ".mesh", ".rgen", ".rint", ".rahit", ".rchit", ".rmiss", ".rcall") -contains $_.Extension

    if ($isShader) {
        $expandedGlsl = "$($_.Directory)/$($_.BaseName).exp$($_.Extension)"
        $expandedSpirv = "$($_.Directory)/$($_.BaseName).exp.spirv"
        $originalSpirv = "$($_.Directory)/$($_.BaseName).spirv"

        ExpandIncludes -Path $_.FullName -Out $expandedGlsl
        CompileShader -Path $expandedGlsl

        Remove-Item $originalSpirv -ErrorAction SilentlyContinue
        Rename-Item $expandedSpirv $originalSpirv
    }
}