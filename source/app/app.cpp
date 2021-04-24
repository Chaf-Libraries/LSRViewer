#define VMA_IMPLEMENTATION
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
	camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 100.f);
	camera.setMovementSpeed(10.f);
	camera.setRotationSpeed(0.1f);
	settings.overlay = true;

	ImGui::StyleColorsDark();

	const std::string font_path = "../data/fonts/arialbd.ttf";
	ImGui::GetIO().Fonts->AddFontFromFileTTF(font_path.c_str(), 20.0f);

	// conditional rendering extension
	enabledDeviceExtensions.push_back(VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME);

	// sampler filter minmax extension for downsampling
	enabledDeviceExtensions.push_back(VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME);

	// dynamic state extension for turn off depth test manually
	enabledDeviceExtensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
	physicalDeviceDepthClipEnableFeatures.depthClipEnable = VK_TRUE;

	// descriptor indexing extension
	enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

	physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
	physicalDeviceDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	physicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
	physicalDeviceDescriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;

	deviceCreatepNextChain = &physicalDeviceDescriptorIndexingFeatures;

	enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

}

Application::~Application()
{
	scene.reset();

	culling_pipeline.reset();
	scene_pipeline.reset();

#ifdef HIZ_ENABLE
	hiz_pipeline.reset();
#endif // HIZ_ENABLE

#ifdef VIS_HIZ
	debug_pipeline.reset();
#endif

	sceneUBO.buffer.destroy();
	last_sceneUBO.buffer.destroy();
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

	const VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.f, 1.f);
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
		}

		// TODO
#ifdef VIS_HIZ
		if (display_debug)
		{
			vkCmdSetDepthTestEnableEXT(drawCmdBuffers[i], VK_FALSE);
			debug_pipeline->buildCommandBuffer(drawCmdBuffers[i], renderPassBeginInfo);
			vkCmdSetDepthTestEnableEXT(drawCmdBuffers[i], VK_TRUE);
		}
#endif // VIS_HIZ

		drawUI(drawCmdBuffers[i]);
		vkCmdEndRenderPass(drawCmdBuffers[i]);



#ifdef USE_OCCLUSION_QUERY
		vkCmdCopyQueryPoolResults(drawCmdBuffers[i], culling_pipeline->query_pool, 0, culling_pipeline->primitive_count, culling_pipeline->query_result_buffer.buffer, 0, sizeof(uint32_t), VK_QUERY_RESULT_WAIT_BIT);
#endif // USE_OCCLUSION_QUERY

		scene_pipeline->copyDepth(drawCmdBuffers[i], depthStencil.image);

		VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
	}
}

void Application::setupDescriptors()
{
	std::vector<VkDescriptorPoolSize> poolSizes = {
	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(scene->materials.size()) * 3)
	};

	const uint32_t maxSetCount = static_cast<uint32_t>(scene->images.size());
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

	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&last_sceneUBO.buffer,
		sizeof(last_sceneUBO.values)));
	VK_CHECK_RESULT(last_sceneUBO.buffer.map());

	updateUniformBuffers();
}

void Application::updateUniformBuffers()
{
	memcpy(last_sceneUBO.buffer.mapped, sceneUBO.buffer.mapped, sizeof(sceneUBO.values));
	memcpy(&last_sceneUBO.values, last_sceneUBO.buffer.mapped, sizeof(sceneUBO.values));


	sceneUBO.values.projection = camera.matrices.perspective;
	sceneUBO.values.view = camera.matrices.view;
	sceneUBO.values.viewPos = camera.viewPos;
	sceneUBO.values.range = { static_cast<float>(width), static_cast<float>(height), camera.getFarClip(), camera.getNearClip() };
	frustum.update(camera.matrices.perspective * camera.matrices.view);
	memcpy(sceneUBO.values.frustum, frustum.planes.data(), sizeof(glm::vec4) * 6);
	memcpy(sceneUBO.buffer.mapped, &sceneUBO.values, sizeof(sceneUBO.values));
}

void Application::prepare()
{
	VulkanExampleBase::prepare();

	vkCmdSetDepthTestEnableEXT = reinterpret_cast<PFN_vkCmdSetDepthTestEnableEXT>(vkGetDeviceProcAddr(device, "vkCmdSetDepthTestEnableEXT"));
	if (!vkCmdSetDepthTestEnableEXT)
	{
		vks::tools::exitFatal("Could not get a valid function pointer for vkCmdSetDepthTestEnableEXT", -1);
	}

	//virtual_texture.prepare(vulkanDevice, 16384, 16384, 1, VK_FORMAT_R8G8B8A8_UNORM, queue);

	//scene = chaf::SceneLoader::LoadFromFile(*vulkanDevice, "D:/Workspace/LSRViewer/data/models/sponza_1/Sponza01.gltf", queue);
	//scene = chaf::SceneLoader::LoadFromFile(*vulkanDevice, "D:/Workspace/LSRViewer/data/models/bonza/Bonza4X.gltf", queue);
	scene = chaf::SceneLoader::LoadFromFile(*vulkanDevice, "D:/Workspace/LSRViewer/data/models/sponza/sponza.gltf", queue);

	scene->buffer_cacher = std::make_unique<chaf::BufferCacher>(*vulkanDevice, queue);

	size_t vertexBufferSize = chaf::SceneLoader::vertex_buffer.size() * sizeof(chaf::Vertex);
	size_t indexBufferSize = chaf::SceneLoader::index_buffer.size() * sizeof(uint32_t);

	scene->buffer_cacher->addBuffer(0, chaf::SceneLoader::vertex_buffer, chaf::SceneLoader::index_buffer);

	culling_pipeline = std::make_unique<CullingPipeline>(*vulkanDevice, *scene);

	scene_pipeline = std::make_unique<ScenePipeline>(*vulkanDevice, *scene);

#ifdef HIZ_ENABLE
	hiz_pipeline = std::make_unique<HizPipeline>(*vulkanDevice, *scene_pipeline);
#endif // HIZ_ENABLE


	scene_pipeline->setupDepth(width, height, depthFormat, queue);

#ifdef HIZ_ENABLE
	hiz_pipeline->prepare(pipelineCache);
#endif // HIZ_ENABLE

	prepareUniformBuffers();
	setupDescriptors();

	scene_pipeline->preparePipelines(pipelineCache, renderPass);

	culling_pipeline->prepareBuffers(queue);

#ifdef HIZ_ENABLE
	culling_pipeline->prepare(pipelineCache, last_sceneUBO.buffer, *hiz_pipeline);
#else
	culling_pipeline->prepare(pipelineCache, sceneUBO.buffer);
#endif

#ifdef VIS_HIZ
	debug_pipeline = std::make_unique<DebugPipeline>(*vulkanDevice);
	debug_pipeline->setupDescriptors(*scene_pipeline);
	debug_pipeline->prepare(pipelineCache, renderPass);
#endif // VIS_HIZ

#ifdef USE_OCCLUSION_QUERY
	pass_sample.resize(culling_pipeline->primitive_count);
#endif

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
	}

	//if(deviceFeatures.fa)
	//VK_EXT_sampler_filter_minmax
	
	if (deviceFeatures.sparseBinding && deviceFeatures.sparseResidencyImage2D) 
	{
		enabledFeatures.shaderResourceResidency = VK_TRUE;
		enabledFeatures.shaderResourceMinLod = VK_TRUE;
		enabledFeatures.sparseBinding = VK_TRUE;
		enabledFeatures.sparseResidencyImage2D = VK_TRUE;
	}
	else 
	{
		std::cout << "Sparse binding not supported" << std::endl;
	}
}

void Application::draw()
{
	VulkanExampleBase::prepareFrame();

#ifdef HIZ_ENABLE
	vkWaitForFences(device, 1, &hiz_pipeline->fence, VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &hiz_pipeline->fence);

	VkSubmitInfo hizSubmitInfo = vks::initializers::submitInfo();
	hizSubmitInfo.commandBufferCount = 1;
	hizSubmitInfo.pCommandBuffers = &hiz_pipeline->command_buffer;
	hizSubmitInfo.signalSemaphoreCount = 1;
	hizSubmitInfo.pSignalSemaphores = &hiz_pipeline->semaphore;
	hizSubmitInfo.waitSemaphoreCount = 0;
	hizSubmitInfo.pWaitSemaphores = nullptr;

	VK_CHECK_RESULT(vkQueueSubmit(hiz_pipeline->compute_queue, 1, &hizSubmitInfo, VK_NULL_HANDLE));

	// Wait on present and compute semaphores
	std::array<VkPipelineStageFlags, 1> cullStageFlags = {
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	};

	VkSubmitInfo cullSubmitInfo = vks::initializers::submitInfo();
	cullSubmitInfo.commandBufferCount = 1;
	cullSubmitInfo.pCommandBuffers = &culling_pipeline->command_buffer;
	cullSubmitInfo.signalSemaphoreCount = 1;
	cullSubmitInfo.pSignalSemaphores = &culling_pipeline->semaphore;
	cullSubmitInfo.waitSemaphoreCount = 1;
	cullSubmitInfo.pWaitSemaphores = &hiz_pipeline->semaphore;
	cullSubmitInfo.pWaitDstStageMask = cullStageFlags.data();

	VK_CHECK_RESULT(vkQueueSubmit(culling_pipeline->compute_queue, 1, &cullSubmitInfo, VK_NULL_HANDLE));
#else
	vkWaitForFences(device, 1, &culling_pipeline->fence, VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &culling_pipeline->fence);

	VkSubmitInfo cullSubmitInfo = vks::initializers::submitInfo();
	cullSubmitInfo.commandBufferCount = 1;
	cullSubmitInfo.pCommandBuffers = &culling_pipeline->command_buffer;
	cullSubmitInfo.signalSemaphoreCount = 1;
	cullSubmitInfo.pSignalSemaphores = &culling_pipeline->semaphore;
	cullSubmitInfo.waitSemaphoreCount = 0;
	cullSubmitInfo.pWaitSemaphores = nullptr;

	VK_CHECK_RESULT(vkQueueSubmit(culling_pipeline->compute_queue, 1, &cullSubmitInfo, VK_NULL_HANDLE));
#endif // HIZ_ENABLE

	//VkSubmitInfo graphicSubmitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
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

#ifdef HIZ_ENABLE
	// Submit to queue
	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, hiz_pipeline->fence));
#else
	// Submit to queue
	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, culling_pipeline->fence));
#endif


	VulkanExampleBase::submitFrame();

	// Get draw count from compute


	//memcpy(culling_pipeline->indirect_status.draw_count.data(), culling_pipeline->conditional_buffer.mapped,sizeof(uint32_t) * culling_pipeline->indirect_status.draw_count.size());

#ifdef _DEBUG
	memcpy(&culling_pipeline->indirect_status.draw_count[0], culling_pipeline->indircet_draw_count_buffer.mapped, sizeof(uint32_t) * culling_pipeline->indirect_status.draw_count.size());
	memcpy(&culling_pipeline->debug_depth.depth[0], culling_pipeline->debug_depth_buffer.mapped, sizeof(float) * culling_pipeline->debug_depth.depth.size());
	memcpy(&culling_pipeline->debug_z.z[0], culling_pipeline->debug_z_buffer.mapped, sizeof(float) * culling_pipeline->debug_z.z.size());

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
	ImGui::Text("frame count: %d", count);
	ImGui::Checkbox("begin benckmark", &begin);

	#if defined(_DEBUG) && defined(VIS_HIZ)
	for (uint32_t i = 0; i < culling_pipeline->primitive_count; i++)
	{
		ImGui::Text("depth: %.3f, maxZ: %.3f , visibility: %d", culling_pipeline->debug_depth.depth[i], culling_pipeline->debug_z.z[i], culling_pipeline->indirect_status.draw_count[i]);
	}
#endif // _DEBUG


#ifdef VIS_HIZ
	std::vector<const char*> hiz_mip_level;
	std::vector<std::string> hiz_mip_level_str;
	for (uint32_t i = 0; i < hiz_pipeline->hiz_image.depth_pyramid_levels; i++)
	{
		hiz_mip_level_str.emplace_back(std::to_string(i + 1));
	}
	hiz_mip_level.reserve(hiz_mip_level_str.size() + 1);
	hiz_mip_level.push_back("0");
	for (size_t i = 0; i < hiz_mip_level_str.size(); i++)
	{
		hiz_mip_level.push_back(hiz_mip_level_str[i].c_str());
	}

	if (ImGui::Combo("Hiz-debug", &display_debug, &hiz_mip_level[0], hiz_mip_level.size(), hiz_mip_level.size()))
	{
		if (display_debug > 0)
		{
			debug_pipeline->updateDescriptors(*hiz_pipeline, display_debug - 1);
			buildCommandBuffers();
		}
	}
#endif // VIS_HIZ

	if (ImGui::Button("screen shot"))
	{
		saveScreenShot();
	}

	if (ImGui::CollapsingHeader("GPU Properties"))
	{

	}

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

void Application::windowResized()
{
	scene_pipeline->resize(width, height, depthFormat, queue);

#ifdef HIZ_ENABLE
	hiz_pipeline->resize();

#endif // HIZ_ENABLE

}

void Application::saveScreenShot()
{
	bool supportsBlit = true;

	// Check blit support for source and destination
	VkFormatProperties formatProps;

	// Check if the device supports blitting from optimal images (the swapchain images are in optimal format)
	vkGetPhysicalDeviceFormatProperties(physicalDevice, swapChain.colorFormat, &formatProps);
	if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
		std::cerr << "Device does not support blitting from optimal tiled images, using copy instead of blit!" << std::endl;
		supportsBlit = false;
	}

	// Check if the device supports blitting to linear images
	vkGetPhysicalDeviceFormatProperties(physicalDevice, VK_FORMAT_R8G8B8A8_UNORM, &formatProps);
	if (!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
		std::cerr << "Device does not support blitting to linear tiled images, using copy instead of blit!" << std::endl;
		supportsBlit = false;
	}

	// Source for the copy is the last rendered swapchain image
	VkImage srcImage = swapChain.images[currentBuffer];

	// Create the linear tiled destination image to copy to and to read the memory from
	VkImageCreateInfo imageCreateCI(vks::initializers::imageCreateInfo());
	imageCreateCI.imageType = VK_IMAGE_TYPE_2D;
	// Note that vkCmdBlitImage (if supported) will also do format conversions if the swapchain color format would differ
	imageCreateCI.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageCreateCI.extent.width = width;
	imageCreateCI.extent.height = height;
	imageCreateCI.extent.depth = 1;
	imageCreateCI.arrayLayers = 1;
	imageCreateCI.mipLevels = 1;
	imageCreateCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateCI.tiling = VK_IMAGE_TILING_LINEAR;
	imageCreateCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	// Create the image
	VkImage dstImage;
	VK_CHECK_RESULT(vkCreateImage(device, &imageCreateCI, nullptr, &dstImage));
	// Create memory to back up the image
	VkMemoryRequirements memRequirements;
	VkMemoryAllocateInfo memAllocInfo(vks::initializers::memoryAllocateInfo());
	VkDeviceMemory dstImageMemory;
	vkGetImageMemoryRequirements(device, dstImage, &memRequirements);
	memAllocInfo.allocationSize = memRequirements.size;
	// Memory must be host visible to copy from
	memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &dstImageMemory));
	VK_CHECK_RESULT(vkBindImageMemory(device, dstImage, dstImageMemory, 0));

	// Do the actual blit from the swapchain image to our host visible destination image
	VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	// Transition destination image to transfer destination layout
	vks::tools::insertImageMemoryBarrier(
		copyCmd,
		dstImage,
		0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

	// Transition swapchain image from present to transfer source layout
	vks::tools::insertImageMemoryBarrier(
		copyCmd,     
		srcImage,
		VK_ACCESS_MEMORY_READ_BIT,
		VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

	// If source and destination support blit we'll blit as this also does automatic format conversion (e.g. from BGR to RGB)
	if (supportsBlit)
	{
		// Define the region to blit (we will blit the whole swapchain image)
		VkOffset3D blitSize;
		blitSize.x = width;
		blitSize.y = height;
		blitSize.z = 1;
		VkImageBlit imageBlitRegion{};
		imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlitRegion.srcSubresource.layerCount = 1;
		imageBlitRegion.srcOffsets[1] = blitSize;
		imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlitRegion.dstSubresource.layerCount = 1;
		imageBlitRegion.dstOffsets[1] = blitSize;

		// Issue the blit command
		vkCmdBlitImage(
			copyCmd,
			srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&imageBlitRegion,
			VK_FILTER_NEAREST);
	}
	else
	{
		// Otherwise use image copy (requires us to manually flip components)
		VkImageCopy imageCopyRegion{};
		imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageCopyRegion.srcSubresource.layerCount = 1;
		imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageCopyRegion.dstSubresource.layerCount = 1;
		imageCopyRegion.extent.width = width;
		imageCopyRegion.extent.height = height;
		imageCopyRegion.extent.depth = 1;

		// Issue the copy command
		vkCmdCopyImage(
			copyCmd,
			srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&imageCopyRegion);
	}

	// Transition destination image to general layout, which is the required layout for mapping the image memory later on
	vks::tools::insertImageMemoryBarrier(
		copyCmd,
		dstImage,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_MEMORY_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

	// Transition back the swap chain image after the blit is done
	vks::tools::insertImageMemoryBarrier(
		copyCmd,
		srcImage,
		VK_ACCESS_TRANSFER_READ_BIT,
		VK_ACCESS_MEMORY_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

	vulkanDevice->flushCommandBuffer(copyCmd, queue);

	// Get layout of the image (including row pitch)
	VkImageSubresource subResource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
	VkSubresourceLayout subResourceLayout;
	vkGetImageSubresourceLayout(device, dstImage, &subResource, &subResourceLayout);

	// Map image memory so we can start copying from it
	const char* data;
	vkMapMemory(device, dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void**)&data);
	data += subResourceLayout.offset;

	auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	struct tm* ptm = localtime(&tt);

	std::string current_time = std::to_string(ptm->tm_year);
	current_time += "_" + std::to_string(ptm->tm_mon);
	current_time += "_" + std::to_string(ptm->tm_mday);
	current_time += "_" + std::to_string(ptm->tm_hour);
	current_time += "_" + std::to_string(ptm->tm_min);
	current_time += "_" + std::to_string(ptm->tm_sec);

	std::ofstream file("../screenshot/screenshot_"+ current_time +".ppm", std::ios::out | std::ios::binary);

	// ppm header
	file << "P6\n" << width << "\n" << height << "\n" << 255 << "\n";

	// If source is BGR (destination is always RGB) and we can't use blit (which does automatic conversion), we'll have to manually swizzle color components
	bool colorSwizzle = false;
	// Check if source is BGR
	// Note: Not complete, only contains most common and basic BGR surface formats for demonstration purposes
	if (!supportsBlit)
	{
		std::vector<VkFormat> formatsBGR = { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SNORM };
		colorSwizzle = (std::find(formatsBGR.begin(), formatsBGR.end(), swapChain.colorFormat) != formatsBGR.end());
	}

	// ppm binary pixel data
	for (uint32_t y = 0; y < height; y++)
	{
		unsigned int* row = (unsigned int*)data;
		for (uint32_t x = 0; x < width; x++)
		{
			if (colorSwizzle)
			{
				file.write((char*)row + 2, 1);
				file.write((char*)row + 1, 1);
				file.write((char*)row, 1);
			}
			else
			{
				file.write((char*)row, 3);
			}
			row++;
		}
		data += subResourceLayout.rowPitch;
	}
	file.close();

	std::cout << "Screenshot saved to disk" << std::endl;

	// Clean up resources
	vkUnmapMemory(device, dstImageMemory);
	vkFreeMemory(device, dstImageMemory, nullptr);
	vkDestroyImage(device, dstImage, nullptr);
}