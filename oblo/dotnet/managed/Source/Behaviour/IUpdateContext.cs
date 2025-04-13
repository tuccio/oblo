using Oblo.Ecs;

namespace Oblo.Behaviour
{
    public interface IUpdateContext
    {
        public Entity Entity { get; }

        public TimeSpan DeltaTime { get; }

        public EntityRegistry EntityRegistry { get; }
    }
}
