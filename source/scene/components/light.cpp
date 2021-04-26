#include <scene/components/light.h>

namespace chaf
{
	LightType Light::getType() const
	{
		return type;
	}

	void Light::setType(LightType type)
	{
		this->type = type;
	}

	const LightProperties& Light::getProperties() const
	{
		return properties;
	}

	void Light::setProperties(LightProperties& properties)
	{
		this->properties = properties;
	}
}