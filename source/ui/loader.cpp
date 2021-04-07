#include <ui/loader.h>
#include <ui/ImGuiFileDialog.h>

#include <framework/platform/filesystem.h>

#include <imgui.h>

namespace chaf
{
	void Loader::draw(ResourceManager& resource_manager)
	{
        bool is_load_scene = true;
        bool has_open = false;

        ImGui::Begin("Loader");
        if (ImGui::Button("Load Scene"))
        {
            igfd::ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", ".gltf,.glb", ".");
            has_open = true;
        }

        if (ImGui::Button("Add Model") && !has_open)
        {
            igfd::ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", ".gltf,.glb", ".");
            is_load_scene = false;
        }

        // display
        if (igfd::ImGuiFileDialog::Instance()->FileDialog("ChooseFileDlgKey"))
        {
            // action if OK
            if (igfd::ImGuiFileDialog::Instance()->IsOk == true)
            {
                std::string filePathName = igfd::ImGuiFileDialog::Instance()->GetFilepathName();
                std::string filePath = igfd::ImGuiFileDialog::Instance()->GetCurrentPath();

                filePathName = filePathName.substr(vkb::fs::path::get(vkb::fs::path::Type::Assets).size(), filePathName.size() - vkb::fs::path::get(vkb::fs::path::Type::Assets).size());
                for (auto& c : filePathName)
                {
                    if (c == '\\')
                    {
                        c = '/';
                    }
                }

                LOGI(filePathName);

                if (is_load_scene)
                {
                    resource_manager.require_new_scene(filePathName);
                }
                // action
            }
            // close
            igfd::ImGuiFileDialog::Instance()->CloseDialog("ChooseFileDlgKey");
        }
        ImGui::End();
	}
}