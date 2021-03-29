#include <ui/inspector.h>

#include <framework/scene_graph/node.h>
#include <framework/scene_graph/component.h>
#include <framework/scene_graph/components/aabb.h>
#include <framework/scene_graph/components/camera.h>
#include <framework/scene_graph/components/perspective_camera.h>
#include <framework/scene_graph/components/orthographic_camera.h>
#include <framework/scene_graph/components/image.h>
#include <framework/scene_graph/components/light.h>
#include <framework/scene_graph/components/material.h>
#include <framework/scene_graph/components/mesh.h>
#include <framework/scene_graph/components/texture.h>
#include <framework/scene_graph/components/pbr_material.h>
#include <framework/scene_graph/components/transform.h>

#include <framework/core/image_view.h>

#include <imgui.h>

#include <glm/gtx/quaternion.hpp>

namespace chaf
{
	void Inspector::draw(vkb::sg::Node* node)
	{
		ImGui::Begin("Inspector");

		if (node == nullptr)
		{
			ImGui::End();
			return;
		}

		ImGui::Text(node->get_name().c_str());
		ImGui::Separator();

		// Transform
		if (node->has_component<vkb::sg::Transform>())
		{
			if (ImGui::CollapsingHeader("Transform"))
			{
				auto& transform = node->get_component<vkb::sg::Transform>();
				glm::vec3 translation = transform.get_translation();
				glm::vec3 rotation = glm::degrees(glm::eulerAngles(transform.get_rotation()));
				glm::vec3 scale = transform.get_scale();

				ImGui::DragFloat3("translation", (float*)(&translation), 0.01f, -std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
				ImGui::DragFloat3("rotation", (float*)(&rotation), 0.01f, -std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
				ImGui::DragFloat3("scale", (float*)(&scale), 0.01f, -std::numeric_limits<float>::max(), std::numeric_limits<float>::max());

				transform.set_translation(translation);
				transform.set_rotation(glm::quat(glm::radians(rotation)));
				transform.set_scale(scale);
			}
		}

		// Camera
		if (node->has_component<vkb::sg::Camera>())
		{
			if (ImGui::CollapsingHeader("Camera"))
			{
				auto& camera = node->get_component<vkb::sg::Camera>();
				ImGui::Text("Camera Type: %s", camera.get_camera_type().name());

				if (camera.get_camera_type() == typeid(vkb::sg::PerspectiveCamera))
				{
					auto& perspective_camera = static_cast<vkb::sg::PerspectiveCamera&>(camera);
					float aspect = perspective_camera.get_aspect_ratio();
					float fov = perspective_camera.get_field_of_view();
					float near_plane = perspective_camera.get_near_plane();
					float far_plane = perspective_camera.get_far_plane();

					ImGui::DragFloat("aspect", &aspect, 0.001f, 0.001f, 180.f);
					ImGui::DragFloat("fov", &fov, 0.001f, 0.001f, 180.f);
					ImGui::DragFloat("near plane", &near_plane, 0.1f, 0.01f, std::numeric_limits<float>::max());
					ImGui::DragFloat("far plane", &far_plane, 0.1f, 0.01f, std::numeric_limits<float>::max());

					perspective_camera.set_aspect_ratio(aspect);
					perspective_camera.set_field_of_view(fov);
					perspective_camera.set_near_plane(near_plane);
					perspective_camera.set_far_plane(far_plane);
				}
				else if (camera.get_camera_type() == typeid(vkb::sg::OrthographicCamera))
				{
					auto& orthographic_camera = static_cast<vkb::sg::OrthographicCamera&>(camera);

					float left = orthographic_camera.get_left();
					float right = orthographic_camera.get_right();
					float top = orthographic_camera.get_top();
					float bottom = orthographic_camera.get_bottom();
					float near_plane = orthographic_camera.get_near_plane();
					float far_plane = orthographic_camera.get_far_plane();

					ImGui::DragFloat("left", &left, 0.001f, -std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
					ImGui::DragFloat("right", &right, 0.001f, -std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
					ImGui::DragFloat("top", &top, 0.001f, -std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
					ImGui::DragFloat("bottom", &bottom, 0.001f, -std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
					ImGui::DragFloat("near_plane", &near_plane, 0.1f, -std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
					ImGui::DragFloat("far_plane", &far_plane, 0.1f, -std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
				}
				else
				{
					ImGui::Text("Unknown camera type");
				}
			}
		}

		// light
		if (node->has_component<vkb::sg::Light>())
		{
			if (ImGui::CollapsingHeader("Light"))
			{
				auto& light = node->get_component<vkb::sg::Light>();

				const char* lightItems[] = { "Directional", "Point", "Spot" };

				vkb::sg::LightType light_type = light.get_light_type();
				vkb::sg::LightProperties light_properties = light.get_properties();

				ImGui::Combo("Light Type", (int*)(&light_type), lightItems, IM_ARRAYSIZE(lightItems));
				ImGui::ColorEdit3("color", (float*)(&light_properties.color));
				ImGui::DragFloat3("direction", (float*)(&light_properties.direction), 0.001f, -std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
				ImGui::DragFloat("intensity", &light_properties.intensity, 0.01f, 0.f, std::numeric_limits<float>::max());
				ImGui::DragFloat("range", &light_properties.range, 0.01f, 0.f, std::numeric_limits<float>::max());
				ImGui::DragFloat("inner_cone_angle", &light_properties.inner_cone_angle, 0.01f, 0.f, std::numeric_limits<float>::max());
				ImGui::DragFloat("outer_cone_angle", &light_properties.outer_cone_angle, 0.01f, 0.f, std::numeric_limits<float>::max());

				light.set_light_type(light_type);
				light.set_properties(light_properties);
			}
		}

		// Mesh
		if (node->has_component<vkb::sg::Mesh>())
		{
			if (ImGui::CollapsingHeader("Mesh"))
			{
				ImGui::Text("Axis-aligned bounding box");

				auto& aabb = node->get_component<vkb::sg::Mesh>().get_bounds();
				ImGui::Text("center: (%f, %f, %f)", aabb.get_center().x, aabb.get_center().y, aabb.get_center().z);
				ImGui::Text("scale: (%f, %f, %f)", aabb.get_scale().x, aabb.get_scale().y, aabb.get_scale().z);
				
				ImGui::Separator();
				for (auto submesh : node->get_component<vkb::sg::Mesh>().get_submeshes())
				{
					uint32_t unname_count = 1;
					ImGui::TextColored({ 255.f,0.f,0.f,255.f }, submesh->get_name().size() ? submesh->get_name().c_str() : ("unname submesh " + std::to_string(unname_count++)).c_str());
					ImGui::Text("vertices count: %d", submesh->vertices_count);
					ImGui::Text("vertices indices: %d", submesh->vertex_indices);
					ImGui::Text("index offset: %d", submesh->index_offset);
					ImGui::Text("vertex attributes:");
					ImGui::Columns(4, "vertex attributes"); // 4-ways, with border
					ImGui::Separator();
					ImGui::Text("name"); ImGui::NextColumn();
					ImGui::Text("format"); ImGui::NextColumn();
					ImGui::Text("stride"); ImGui::NextColumn();
					ImGui::Text("offset"); ImGui::NextColumn();
					ImGui::Separator();

					for (auto attribute : submesh->get_attributes())
					{
						ImGui::Text(attribute.first.c_str()); ImGui::NextColumn();
						ImGui::Text(vkb::to_string(attribute.second.format).c_str()); ImGui::NextColumn();
						ImGui::Text("%d", attribute.second.stride); ImGui::NextColumn();
						ImGui::Text("%d", attribute.second.offset); ImGui::NextColumn();
					}
					ImGui::Columns(1);
					ImGui::Separator();

					ImGui::Text("material:");
					auto material = static_cast<const vkb::sg::PBRMaterial*>(submesh->get_material());
					
					std::unordered_map<vkb::sg::AlphaMode, const char*> alpha_mode_map = {
						{vkb::sg::AlphaMode::Opaque, "Opaque"},
						{vkb::sg::AlphaMode::Mask, "Mask"},
						{vkb::sg::AlphaMode::Blend, "Blend"}
					};

					ImGui::Bullet(); ImGui::Text("base color: (%f, %f, %f, %f)", material->base_color_factor.r, material->base_color_factor.g, material->base_color_factor.b, material->base_color_factor.a);
					ImGui::Bullet(); ImGui::Text("metallic factor: %f", material->metallic_factor);
					ImGui::Bullet(); ImGui::Text("roughness factor: %f", material->roughness_factor);
					ImGui::Bullet(); ImGui::Text("emissive: (%f, %f, %f)", material->emissive.x, material->emissive.y, material->emissive.z);
					ImGui::Bullet(); ImGui::Text("double sided: %s", material->double_sided ? "true" : "false");
					ImGui::Bullet(); ImGui::Text("alpha cutoff: %f", material->alpha_cutoff);
					ImGui::Bullet(); ImGui::Text("alpha mode: %s", alpha_mode_map.at(material->alpha_mode));
					if (ImGui::TreeNode("texture"))
					{
						for (auto texture : material->textures)
						{
								ImGui::Bullet();
								ImGui::Text(texture.first.c_str());
						}
						ImGui::TreePop();
					}


					ImGui::Separator();
				}
			}
		}

		ImGui::End();
	}
}