using NUnit.Framework;
using System.IO;
using System.IO.Compression;

namespace Unity.MeshSyncDCCPlugin {

class PluginsTest {

	[Test]
	public void DCCPluginsExist() {
		
		string path = Path.Combine("Packages", "com.unity.meshsync.dcc-plugins","Editor","Plugins");
		path = Path.GetFullPath(path);
		int numFiles = Directory.GetFiles(path, "*", SearchOption.TopDirectoryOnly).Length;
		Assert.Greater(numFiles,0,"There are no DCC plugins");
	}

	[Test]
	public void Blender5WindowsPluginsExist() {
		string path = Path.GetFullPath(Path.Combine("Packages", "com.unity.meshsync.dcc-plugins", "Editor", "Plugins", "UnityMeshSync_Blender_Windows.zip"));
		using (ZipArchive archive = new ZipArchive(File.OpenRead(path), ZipArchiveMode.Read)) {
			Assert.IsNotNull(archive.GetEntry("UnityMeshSync_0.17.1-preview_Blender_Windows/blender-5.0.0.zip"));
			Assert.IsNotNull(archive.GetEntry("UnityMeshSync_0.17.1-preview_Blender_Windows/blender-5.1.1.zip"));
		}
	}

}
	
} //end namespace
