#include <app/app.h>

#include <iostream>

#ifdef _DEBUG
#define ENABLE_VALIDATION true
#else
#define ENABLE_VALIDATION false
#endif


Application::Application(): VulkanExampleBase(ENABLE_VALIDATION)
{
	title = "LSRViewer";
	camera.type = Camera::CameraType::firstperson;
	camera.flipY = true;
	camera.setPosition(glm::vec3(0.0f, 1.0f, 0.0f));
	camera.setRotation(glm::vec3(0.0f, -90.0f, 0.0f));
	camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 10000.f);
	camera.setMovementSpeed(10.f);
	camera.setRotationSpeed(0.1f);
	settings.overlay = true;

	ImGui::StyleColorsDark();

	const std::string font_path = "../data/fonts/arialbd.ttf";
	ImGui::GetIO().Fonts->AddFontFromFileTTF(font_path.c_str(), 20.0f);

	enabledDeviceExtensions.push_back(VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME);
}

Application::~Application()
{
	scene.reset();

	culling_pipeline.reset();
	scene_pipeline.reset();

	sceneUBO.buffer.destroy();
}

void Application::buildCommandBuffers()
{
	VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

	VkClearValue clearValues[2];
	clearValues[0].color = defaultClearColor;
	clearValues[0].color = { { 0.25f, 0.25f, 0.25f, 1.0f } };;
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = width;
	renderPassBeginInfo.renderArea.extent.height = height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	const VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
	const VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);

	for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
	{
		renderPassBeginInfo.framebuffer = frameBuffers[i];
		VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

#ifdef USE_OCCLUSION_QUERY
		vkCmdResetQueryPool(drawCmdBuffers[i], culling_pipeline->query_pool, 0, culling_pipeline->primitive_count);
#endif // USE_OCCLUSION_QUERY

		vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
		vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);
		
		{
			// CPU culling
			// scene_pipeline->bindCommandBuffers(drawCmdBuffers[i], frustum);

			// GPU culling
			scene_pipeline->bindCommandBuffers(drawCmdBuffers[i], *culling_pipeline);

			// Test
			 // scene_pipeline->bindCommandBuffers(drawCmdBuffers[i], sceneUBO.values.frustum, *culling_pipeline);

		}

		drawUI(drawCmdBuffers[i]);
		vkCmdEndRenderPass(drawCmdBuffers[i]);


#ifdef USE_OCCLUSION_QUERY
		vkCmdCopyQueryPoolResults(drawCmdBuffers[i], culling_pipeline->query_pool, 0, culling_pipeline->primitive_count, culling_pipeline->query_result_buffer.buffer, 0, sizeof(uint32_t), VK_QUERY_RESULT_WAIT_BIT);
#endif // USE_OCCLUSION_QUERY


		VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
	}
}

void Application::setupDescriptors()
{
	std::vector<VkDescriptorPoolSize> poolSizes = {
	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2),
	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(scene->materials.size()) * 2),
	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4),
	};

	const uint32_t maxSetCount = static_cast<uint32_t>(scene->images.size()) + 1;
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, maxSetCount);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

	scene_pipeline->setupDescriptors(descriptorPool, sceneUBO.buffer);
}

void Application::prepareUniformBuffers()
{
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&sceneUBO.buffer,
		sizeof(sceneUBO.values)));
	VK_CHECK_RESULT(sceneUBO.buffer.map());
	updateUniformBuffers();
}

void Application::updateUniformBuffers()
{
	sceneUBO.values.projection = camera.matrices.perspective;
	sceneUBO.values.view = camera.matrices.view;
	sceneUBO.values.viewPos = camera.viewPos;
	frustum.update(camera.matrices.perspective * camera.matrices.view);
	memcpy(sceneUBO.values.frustum, frustum.planes.data(), sizeof(glm::vec4) * 6);
	memcpy(sceneUBO.buffer.mapped, &sceneUBO.values, sizeof(sceneUBO.values));
}

void Application::prepare()
{
	VulkanExampleBase::prepare();

	scene = chaf::SceneLoader::LoadFromFile(*vulkanDevice, "D:/Workspace/LSRViewer/data/models/sponza/sponza.gltf", queue);

	scene->buffer_cacher = std::make_unique<chaf::BufferCacher>(*vulkanDevice, queue);

	size_t vertexBufferSize = chaf::SceneLoader::vertex_buffer.size() * sizeof(chaf::Vertex);
	size_t indexBufferSize = chaf::SceneLoader::index_buffer.size() * sizeof(uint32_t);

	scene->buffer_cacher->addBuffer(0, chaf::SceneLoader::vertex_buffer, chaf::SceneLoader::index_buffer);

	//while (scene->buffer_cacher->isBusy()) {}

	culling_pipeline = std::make_unique<CullingPipeline>(*vulkanDevice, *scene);

	scene_pipeline = std::make_unique<ScenePipeline>(*vulkanDevice, *scene);

	prepareUniformBuffers();
	setupDescriptors();

	scene_pipeline->preparePipelines(pipelineCache, renderPass);

	culling_pipeline->prepareBuffers(queue);
	culling_pipeline->prepare(pipelineCache, descriptorPool, sceneUBO.buffer);

	pass_sample.resize(culling_pipeline->primitive_count);

	buildCommandBuffers();
	prepared = true;
}

void Application::getEnabledFeatures()
{
	if (deviceFeatures.multiDrawIndirect) 
	{
		enabledFeatures.multiDrawIndirect = VK_TRUE;
	}
	if (deviceFeatures.fillModeNonSolid) 
	{
		enabledFeatures.fillModeNonSolid = VK_TRUE;
	};
}

void Application::draw()
{
	VulkanExampleBase::prepareFrame();

	culling_pipeline->submit();

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];

	// Wait on present and compute semaphores
	std::array<VkPipelineStageFlags, 2> stageFlags = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	};
	std::array<VkSemaphore, 2> waitSemaphores = {
		semaphores.presentComplete,						// Wait for presentation to finished
		culling_pipeline->semaphore								// Wait for compute to finish
	};

	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
	submitInfo.pWaitDstStageMask = stageFlags.data();

	// Submit to queue
	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, culling_pipeline->fence));

	VulkanExampleBase::submitFrame();

	// Get draw count from compute
	memcpy(&culling_pipeline->indirect_status.draw_count[0], culling_pipeline->indircet_draw_count_buffer.mapped, sizeof(uint32_t)* culling_pipeline->indirect_status.draw_count.size());

	//memcpy(culling_pipeline->indirect_status.draw_count.data(), culling_pipeline->conditional_buffer.mapped,sizeof(uint32_t) * culling_pipeline->indirect_status.draw_count.size());

#ifdef _DEBUG
	for (auto& node : scene->getNodes())
	{
		if (node->hasComponent<chaf::Mesh>())
		{
			auto& mesh = node->getComponent<chaf::Mesh>();
			for (uint32_t i = 0; i < mesh.getPrimitives().size(); i++)
			{
				mesh.getPrimitives()[i].visible = culling_pipeline->indirect_status.draw_count[culling_pipeline->id_lookup[node->getID()][i]];
			}
		}
	}

#endif // _DEBUG


}

void Application::render()
{
	draw();

	if (camera.updated) 
	{
		updateUniformBuffers();
		//buildCommandBuffers();
	}
}

void Application::OnUpdateUIOverlay(vks::UIOverlay* overlay)
{

}

void Application::updateOverlay()
{
	if (!settings.overlay)
		return;

	ImGuiIO& io = ImGui::GetIO();

	io.DisplaySize = ImVec2((float)width, (float)height);
	io.DeltaTime = frameTimer;

	io.MousePos = ImVec2(mousePos.x, mousePos.y);
	io.MouseDown[0] = mouseButtons.left;
	io.MouseDown[1] = mouseButtons.right;

	ImGui::NewFrame();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::SetNextWindowPos(ImVec2(10, 10));
	ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
	ImGui::Begin((title + " GUI").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

	// Status
	static uint32_t last_fps = lastFPS;
	static uint32_t ave_fps = lastFPS;
	static uint32_t count = 0;
	static bool begin{ false };

	if (lastFPS != last_fps&& lastFPS>0&&begin)
	{
		ave_fps = (ave_fps * count + lastFPS) / (count+1);
		count++;
		last_fps = lastFPS;
	}
	
	ImGui::TextUnformatted((std::string("GPU: ") + deviceProperties.deviceName).c_str());
	ImGui::Text("%.2f ms/frame (%.1d fps)", (1000.0f / lastFPS), lastFPS);
	ImGui::Text("ave fps: %.1d", ave_fps);
	ImGui::Checkbox("Begin", &begin);

	// Camera Controller
	if (ImGui::CollapsingHeader("Main Camera"))
	{
		ImGui::DragFloat("move speed", &camera.movementSpeed, 0.01f, 0.01f, 10.f, "%.2f");
		camera.setMovementSpeed(camera.movementSpeed);
		ImGui::DragFloat("rotate speed", &camera.rotationSpeed, 0.01f, 0.01f, 10.f, "%.2f");
		camera.setRotationSpeed(camera.rotationSpeed);
		ImGui::Text("near clip: %f", camera.getNearClip());
		ImGui::Text("far clip: %f", camera.getFarClip());
	}

	// Scene Hierarchy
	static chaf::Node* selected_node = nullptr;
	if (ImGui::CollapsingHeader("Scene"))
	{
		std::function<void(chaf::Node* node, uint32_t unname)> drawNode =
			[&drawNode, this](chaf::Node* node, uint32_t unname) {
			if (!node)
			{
				return;
			}

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));
			bool open = ImGui::TreeNodeEx(std::to_string(node->getID()).c_str(),
				ImGuiTreeNodeFlags_FramePadding | (node->getChildren().size() ? 0 : ImGuiTreeNodeFlags_Leaf) | ((node == selected_node) ? ImGuiTreeNodeFlags_Selected : 0),
				"%s", ((node->getName().size() > 0 ? node->getName() : ("unname_" + std::to_string(unname++))) + (node->visibility ? "" : " (culled)")).c_str());
			ImGui::PopStyleVar();

			if (ImGui::IsItemClicked())
			{
				selected_node = node;
			}

			if (open)
			{
				if (node)
				{
					auto children = node->getChildren();
					for (auto& child : children)
					{
						drawNode(child, unname);
					}
				}
				ImGui::TreePop();
			}
		};

		uint32_t unname = 1;

		auto& children = scene->getNodes();
		for (auto& child : children)
		{
			drawNode(&*child, unname);
		}
	}
	else
	{
		selected_node = nullptr;
	}

	// Components
	if (selected_node && ImGui::CollapsingHeader("Components"))
	{
		// Transform
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));
		bool open = ImGui::TreeNodeEx("Transform Components",
			ImGuiTreeNodeFlags_FramePadding,
			"%s", "Transform");
		ImGui::PopStyleVar();
		if (open)
		{
			auto& transform = selected_node->getComponent<chaf::Transform>();
			auto& translation = transform.getTranslation();
			auto& rotation = glm::eulerAngles(transform.getRotation());
			auto& scale = transform.getScale();
			ImGui::Text("position: (%f, %f, %f)", translation.x, translation.y, translation.z);
			ImGui::Text("rotation: (%f, %f, %f)", rotation.x, rotation.y, rotation.z);
			ImGui::Text("scale: (%f, %f, %f)", scale.x, scale.y, scale.z);
			ImGui::TreePop();
		}

		// Mesh
		if (selected_node->hasComponent<chaf::Mesh>())
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));
			bool open = ImGui::TreeNodeEx("Mesh Components",
				ImGuiTreeNodeFlags_FramePadding,
				"%s", "Mesh");
			ImGui::PopStyleVar();
			if (open)
			{
				auto& mesh = selected_node->getComponent<chaf::Mesh>();

				ImGui::Text("Primitive count: %d", mesh.getPrimitives().size());
				ImGui::Text("Axis Aligned Bounding Box: ");
				auto center = mesh.getBounds().getCenter();
				auto scale = mesh.getBounds().getScale();
				ImGui::Text("center: (%f, %f, %f)", center.x, center.y, center.z);
				ImGui::Text("scale: (%f, %f, %f)", scale.x, scale.y, scale.z);

				for (auto& primitive : mesh.getPrimitives())
				{
					ImGui::Separator();
					ImGui::Text("Primitive %ld", primitive.id);
					ImGui::Text("Axis Aligned Bounding Box: ");
					auto center = primitive.bbox.getCenter();
					auto scale = primitive.bbox.getScale();
					auto min = primitive.bbox.getMin();
					auto max = primitive.bbox.getMax();
					ImGui::Text("center: (%f, %f, %f)", center.x, center.y, center.z);
					ImGui::Text("scale: (%f, %f, %f)", scale.x, scale.y, scale.z);

					chaf::AABB world_bounds = { primitive.bbox.getMin(), primitive.bbox.getMax() };

					world_bounds.transform(selected_node->getComponent<chaf::Transform>().getWorldMatrix());

					ImGui::Text("min: (%f, %f, %f)", world_bounds.getMin().x, world_bounds.getMin().y, world_bounds.getMin().z);
					ImGui::Text("max: (%f, %f, %f)", world_bounds.getMax().x, world_bounds.getMax().y, world_bounds.getMax().z);
					ImGui::PushID(static_cast<int>(primitive.id));
					ImGui::Checkbox("visibility", &primitive.visible);
					ImGui::PopID();
				}
				ImGui::TreePop();
			}
		}

	}

	ImGui::PushItemWidth(110.0f * UIOverlay.scale);
	OnUpdateUIOverlay(&UIOverlay);
	ImGui::PopItemWidth();

	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::Render();

	if (UIOverlay.update() || UIOverlay.updated || scene->buffer_cacher->updated)
	{
		buildCommandBuffers();
		UIOverlay.updated = false;
		scene->buffer_cacher->updated = false;
	}
}