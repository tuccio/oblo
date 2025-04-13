namespace Oblo.Ecs
{
    public static class ComponentTraits<T> where T : IComponent
    {
        public static ComponentTypeId TypeId { get; } = FindComponentType();

        private static ComponentTypeId FindComponentType()
        {
            var nativeTypeField = typeof(T).GetField("NativeType", System.Reflection.BindingFlags.Static | System.Reflection.BindingFlags.NonPublic);
            var nativeType = nativeTypeField?.GetValue(null) as string;

            var id = nativeType is not null ? Bindings.oblo_ecs_find_component_type(nativeType) : default;

            return new ComponentTypeId(id);
        }
    }
}
