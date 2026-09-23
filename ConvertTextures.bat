@echo off

cd /d"%~dp0\Resources\Textures"

for %%f in  (*.png) do (
	..\..\Tools\texconv.exe -m 0 -f BC1_UNORM -pow2 -y -o "..\Textures" "%%f"
)

for /R %%f in (*.jpg) do (
	..\..\Tools\texconv.exe -m 0 -f BC1_UNORM -pow2 -y -o "%~dp0\Resources\Textures" "%%f"
)

cd /d"%~dp0\Resources\Edit"

for %%f in  (*.png) do (
	..\..\Tools\texconv.exe -m 0 -f BC1_UNORM -pow2 -y -o "..\Edit" "%%f"
)

cd /d"%~dp0\Resources\Textures\Fonts"

for %%f in  (*.png) do (
	..\..\..\Tools\texconv.exe -m 0 -f R8G8B8A8_UNORM -y -o "..\..\Textures" "%%f"
)
