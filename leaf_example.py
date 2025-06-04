import torch
import torch.nn as nn
import torch.optim as optim
import torchvision
import torchvision.transforms as transforms
from torch.utils.data import DataLoader
from torchvision.models import resnet50
import time
import wandb
from leaf_decorator import leaf

# Your existing model definition
class ResNetModel(nn.Module):
    def __init__(self):
        super().__init__()
        self.model = resnet50(pretrained=True)
        self.model.conv1 = nn.Conv2d(3, 64, kernel_size=3, stride=1, padding=1, bias=False)
        self.model.maxpool = nn.Identity()
        self.model.fc = nn.Linear(self.model.fc.in_features, 10)
    
    def forward(self, x):
        return self.model(x)

# Your existing training function, now with the @leaf decorator
@leaf(num_gpus=2)  # Just add this line to make it distributed!
def train(model, dataloader, criterion, optimizer, trainer=None):
    model.train()
    running_loss = 0.0
    correct = 0
    total = 0
    
    for batch_idx, (inputs, targets) in enumerate(dataloader):
        # If using leaf training, use the trainer
        if trainer is not None:
            loss = trainer.train_step(
                inputs=inputs,
                targets=targets,
                criterion=criterion,
                optimizer=optimizer
            )
        else:
            # Original training code
            inputs, targets = inputs.to(device), targets.to(device)
            optimizer.zero_grad()
            outputs = model(inputs)
            loss = criterion(outputs, targets)
            loss.backward()
            optimizer.step()
        
        running_loss += loss.item()
        _, predicted = outputs.max(1)
        total += targets.size(0)
        correct += predicted.eq(targets).sum().item()
        
        if (batch_idx + 1) % 100 == 0:
            print(f'Batch: {batch_idx + 1} | Loss: {running_loss/(batch_idx + 1):.3f} | '
                  f'Acc: {100.*correct/total:.2f}%')
    
    return running_loss/len(dataloader), 100.*correct/total

# Main training setup
def main():
    # Set device
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    
    # Hyperparameters
    num_epochs = 50
    batch_size = 128
    learning_rate = 0.001
    
    # Data preprocessing
    transform_train = transforms.Compose([
        transforms.RandomCrop(32, padding=4),
        transforms.RandomHorizontalFlip(),
        transforms.ToTensor(),
        transforms.Normalize((0.4914, 0.4822, 0.4465), (0.2023, 0.1994, 0.2010)),
    ])
    
    # Load dataset
    trainset = torchvision.datasets.CIFAR10(root='./data', train=True,
                                          download=True, transform=transform_train)
    trainloader = DataLoader(trainset, batch_size=batch_size,
                           shuffle=True, num_workers=2)
    
    # Create model and training components
    model = ResNetModel()
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters(), lr=learning_rate)
    
    # Initialize wandb
    wandb.init(project="leaf-training")
    
    # Training loop
    print('Starting training...')
    for epoch in range(num_epochs):
        start_time = time.time()
        
        train_loss, train_acc = train(
            model=model,
            dataloader=trainloader,
            criterion=criterion,
            optimizer=optimizer
        )
        
        print(f'\nEpoch: {epoch + 1}/{num_epochs}')
        print(f'Time: {time.time() - start_time:.2f}s')
        print(f'Train Loss: {train_loss:.3f} | Train Acc: {train_acc:.2f}%')
        
        # Log metrics
        wandb.log({
            "epoch": epoch,
            "train_loss": train_loss,
            "train_acc": train_acc
        })

if __name__ == "__main__":
    main() 