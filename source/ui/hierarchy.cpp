#include <ui/hierarchy.h>

#include <framework/scene_graph/node.h>
#include <framework/scene_graph/components/mesh.h>
#include <framework/scene_graph/components/sub_mesh.h>

#include <imgui.h>

using namespace vkb;

namespace chaf
{
	vkb::sg::Node* Hierarchy::selected_node = nullptr;
	vkb::sg::Node* Hierarchy::root = nullptr;
	uint32_t Hierarchy::vertices_count = 0;
	uint32_t Hierarchy::triangle_count = 0;

	void Hierarchy::draw(vkb::sg::Scene* scene)
	{
		auto& root = scene->get_root_node();

		uint32_t unname = 1;

		vertices_count = 0;
		triangle_count = 0;

		ImGui::Begin("Hierachy");
		if (scene != nullptr)
		{
			draw_node(&root, unname);
		}
		ImGui::Separator();
		ImGui::Text("Total vertices count: %d", vertices_count);
		ImGui::Text("Total triangles count: %d", triangle_count);
		ImGui::End();
	}

	void Hierarchy::reset_selected()
	{
		Hierarchy::selected_node = nullptr;
	}

	vkb::sg::Node* Hierarchy::get_selected()
	{
		return selected_node;
	}

	void Hierarchy::draw_node(vkb::sg::Node* node, uint32_t& unname)
	{
		if (!node)return;

		if (node->has_component<vkb::sg::Mesh>())
		{
			auto& mesh = node->get_component<vkb::sg::Mesh>();
			for (auto sub_mesh : mesh.get_submeshes())
			{
				vertices_count += sub_mesh->vertices_count;
				triangle_count += sub_mesh->vertex_indices / 3;
			}
		}

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));
		bool open = ImGui::TreeNodeEx(std::to_string(node->get_id()).c_str(),
			ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_DefaultOpen
			| ((node == selected_node) ? ImGuiTreeNodeFlags_Selected : 0) | (node->get_children().size() ? 0 : ImGuiTreeNodeFlags_Leaf),
			"%s", node->get_name().size() > 0 ? node->get_name().c_str() : ("unname" + std::to_string(unname++)).c_str());
		ImGui::PopStyleVar();

		//ImGui::PushID(std::to_string(node->get_id()).c_str());
		//if (ImGui::BeginPopupContextItem())
		//{
		//	ImGui::Text(node->get_name().c_str());
		//	ImGui::Separator();
		//	//	delete
		//	/*if (!(node == root))
		//	{
		//		if (ImGui::MenuItem("Delete"))
		//			RemoveEntity(node);
		//	}*/
		//	//	rename
		//	//if (node)
		//	//{
		//	//	ImGui::Separator();
		//	//	EditorBasic::Rename(node);
		//	//}
		//	ImGui::EndPopup();
		//}
		//ImGui::PopID();

		//	left click check
		if (ImGui::IsItemClicked())
		{
			selected_node = node;
		}

		//	Drag&Drop Effect
		//EditorBasic::SetDragDropSource<Entity>("Hierarchy Drag&Drop", [&]() {
		//	ImGui::Text(node.GetComponent<TagComponent>().Tag.c_str());
		//	}, EditorBasic::GetSelectEntity());

		//EditorBasic::SetDragDropTarget<Entity>("Hierarchy Drag&Drop", [&](Entity srcNode) {
		//	srcNode.MoveAsChildOf(node);
		//	auto& transform = srcNode.GetComponent<TransformComponent>();
		//	transform.SetRelatePosition(srcNode.GetParent().GetComponent<TransformComponent>().Position);
		//	});
		//	Recurse
		if (open)
		{
			if (node)
			{
				auto children = node->get_children();
				for (auto& child : children)
				{
					draw_node(child, unname);
				}
			}
			ImGui::TreePop();
		}
	}

}